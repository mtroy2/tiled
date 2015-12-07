/*
 * objectgroup.cpp
 * Copyright 2008, Roderic Morris <roderic@ccs.neu.edu>
 * Copyright 2008-2014, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2009-2010, Jeff Bland <jksb@member.fsf.org>
 *
 * This file is part of libtiled.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "objectgroup.h"

#include "layer.h"
#include "map.h"
#include "mapobject.h"
#include "tile.h"

#include <cmath>

using namespace Tiled;

ObjectGroup::ObjectGroup()
    : Layer(ObjectGroupType, QString(), 0, 0, 0, 0)
    , mDrawOrder(TopDownOrder)
{
}

ObjectGroup::ObjectGroup(const QString &name,
                         int x, int y, int width, int height)
    : Layer(ObjectGroupType, name, x, y, width, height)
    , mDrawOrder(TopDownOrder)
{
}

ObjectGroup::~ObjectGroup()
{
    qDeleteAll(mObjects);
}

void ObjectGroup::addObject(MapObject *object)
{

    mObjects.append(object);
    object->setObjectGroup(this);
    if (mMap && object->id() == 0)
        object->setId(mMap->takeNextObjectId());
}

void ObjectGroup::insertObject(int index, MapObject *object)
{
    mObjects.insert(index, object);
    sort(mObjects);
    object->setObjectGroup(this);
    if (mMap && object->id() == 0)
        object->setId(mMap->takeNextObjectId());
}

int ObjectGroup::removeObject(MapObject *object)
{
    const int index = mObjects.indexOf(object);
    Q_ASSERT(index != -1);

    mObjects.removeAt(index);
    object->setObjectGroup(nullptr);
    return index;
}

void ObjectGroup::removeObjectAt(int index)
{
    MapObject *object = mObjects.takeAt(index);
    object->setObjectGroup(nullptr);
}

void ObjectGroup::moveObjects(int from, int to, int count)
{
    // It's an error when 'to' lies within the moving range of objects
    Q_ASSERT(count >= 0);
    Q_ASSERT(to <= from || to >= from + count);

    // Nothing to be done when 'to' is the start or the end of the range, or
    // when the number of objects to be moved is 0.
    if (to == from || to == from + count || count == 0)
        return;

    const QList<MapObject*> movingObjects = mObjects.mid(from, count);
    mObjects.erase(mObjects.begin() + from,
                   mObjects.begin() + from + count);

    if (to > from)
        to -= count;

    for (int i = 0; i < count; ++i)
        mObjects.insert(to + i, movingObjects.at(i));
}

QRectF ObjectGroup::objectsBoundingRect() const
{
    QRectF boundingRect;
    foreach (const MapObject *object, mObjects)
        boundingRect = boundingRect.united(object->bounds());
    return boundingRect;
}

bool ObjectGroup::isEmpty() const
{
    return mObjects.isEmpty();
}

QSet<SharedTileset> ObjectGroup::usedTilesets() const
{
    QSet<SharedTileset> tilesets;

    foreach (const MapObject *object, mObjects)
        if (const Tile *tile = object->cell().tile)
            tilesets.insert(tile->sharedTileset());

    return tilesets;
}

bool ObjectGroup::referencesTileset(const Tileset *tileset) const
{
    foreach (const MapObject *object, mObjects) {
        const Tile *tile = object->cell().tile;
        if (tile && tile->tileset() == tileset)
            return true;
    }

    return false;
}

void ObjectGroup::replaceReferencesToTileset(Tileset *oldTileset,
                                             Tileset *newTileset)
{
    foreach (MapObject *object, mObjects) {
        const Tile *tile = object->cell().tile;
        if (tile && tile->tileset() == oldTileset) {
            Cell cell = object->cell();
            cell.tile = newTileset->tileAt(tile->id());
            object->setCell(cell);
        }
    }
}

void ObjectGroup::offsetObjects(const QPointF &offset,
                                const QRectF &bounds,
                                bool wrapX, bool wrapY)
{
    foreach (MapObject *object, mObjects) {
        const QPointF objectCenter = object->bounds().center();
        if (!bounds.contains(objectCenter))
            continue;

        QPointF newCenter(objectCenter + offset);

        if (wrapX && bounds.width() > 0) {
            qreal nx = std::fmod(newCenter.x() - bounds.left(), bounds.width());
            newCenter.setX(bounds.left() + (nx < 0 ? bounds.width() + nx : nx));
        }

        if (wrapY && bounds.height() > 0) {
            qreal ny = std::fmod(newCenter.y() - bounds.top(), bounds.height());
            newCenter.setY(bounds.top() + (ny < 0 ? bounds.height() + ny : ny));
        }

        object->setPosition(object->position() + (newCenter - objectCenter));
    }
}

bool ObjectGroup::canMergeWith(Layer *other) const
{
    return other->isObjectGroup();
}
void ObjectGroup::sort(QList<MapObject *> &list)
{

    int maxDepth = (int) std::floor( std::log2(list.size()) ) * 2;
    int end = list.size();
    QVector<MapObject *> vec = list.toVector();
    introSort(vec, maxDepth, 0, end);
    list.clear();
    list = QList<MapObject *>::fromVector(vec);

}
void ObjectGroup::introSort(QVector<MapObject *> &vec, int maxDepth, int begin, int end){
   const int n = vec.size();
   // base case
   if (n <= 1)
   {
        return;
   }
   else if (n == 2)
   {
       const QString first = vec[0]->name();
       const QString  second = vec[1]->name();
       if(QString::compare(first,second, Qt::CaseInsensitive) >= 0)
       {
           MapObject * temp = vec[0];
           vec[0] = vec[1];
           vec[1] = temp;
       }

   }
   else if( maxDepth == 0 ){
       heapSort(vec,begin, end);
   }
   else
   {
       int p = partition(vec, begin, end);

       introSort(vec, maxDepth - 1, begin, p);
       introSort(vec, maxDepth -1, p + 1, end);

   }
}
void ObjectGroup::heapSort(QVector<MapObject *> &vec, int begin, int end)
{

    heapify(vec,begin, end);
    int last = end - 1;
    while (last > 0)
    {
        MapObject * temp = vec[begin];
        vec[begin] = vec[last];
        vec[last] = temp;
        end--;
        siftDown(vec, begin,end);
    }
}
void ObjectGroup::heapify(QVector<MapObject *> &vec, int begin, int end)
{
    int start =  begin + std::floor( (end - 2) / 2 );
    while (start >= begin)
    {
        siftDown(vec,begin,end - 1);
        start--;
    }
}
void ObjectGroup::siftDown(QVector<MapObject *> &vec, int siftBegin, int siftEnd)
{
    int root = siftBegin;
    while ( (root*2 + 1) <= siftEnd)
    {
        int child = root*2 + 1;
        int swap = root;
        if (vec[swap] < vec[child])
        {
            swap = child;
        }
        else if ( child+1 <= siftEnd && vec[swap] < vec[child+1])
        {
             swap = child + 1;
        }
        else if (swap == root)
        {
            return;
        }
        else
        {
            MapObject *temp = vec[root];
            vec[root] = vec[swap];
            vec[swap] = temp;
            root = swap;
        }
    }
}

int ObjectGroup::partition(QVector<MapObject *> &vec, int begin, int end)
{
    MapObject *first = vec[begin];
    MapObject *last = vec[end - 1];
    MapObject *middle = vec[ begin + ((end-begin) / 2) ];
    const QString start = first->name();
    const QString midway = middle->name();
    const QString finish = last->name();
    // start > finish
    if (QString::compare(start, finish, Qt::CaseInsensitive) >= 0)
    {
        // start > finish & start > midway
        if (QString::compare(start, midway, Qt::CaseInsensitive) >= 0)
        {
            // start > finish & start > midway & midway > finish
            // midway is the median (Order is : Finish, Midway, Start)
            if (QString::compare(midway, finish, Qt::CaseInsensitive) >= 0)
            {
                MapObject * temp = first;
                vec[0] = last;
                vec[vec.size()-1] = temp;
                return reorder(vec, middle,begin,end);

            }
            // else finish is median
            //(Order is : Midway, Finish, Start)
            else
            {
                MapObject * temp1 = first;
                MapObject * temp2 = middle;
                vec[0] = temp2;
                vec[begin + ((end-begin) / 2)] = last;
                vec[vec.size()-1] = temp1;
                return reorder(vec,last,begin,end);

            }
        }
        // start is midpoint
        // (Order is : Finish, start, midway)
        else
        {
            MapObject * temp1 = first;
            MapObject * temp2 = middle;
            vec[0] = last;
            vec[begin + ((end-begin) / 2)] = temp1;
            vec[vec.size()-1] = temp2;
            return reorder(vec, first,begin,end);

        }
    }
    // start < finish
    else
    {
        // Start < Finish & Start < Midway
        if (QString::compare(start, midway, Qt::CaseInsensitive) <= 0)
        {
            // Start < Finish & Start < Midway & Midway > Finish
            // Order is ( Start, Finish, Midway)
            if (QString::compare(midway, finish, Qt::CaseInsensitive) >= 0)
            {
                MapObject * temp1 = middle;
                vec[begin + ((end-begin) / 2)] = last;
                vec[vec.size()-1] = temp1;
                return reorder(vec, last,begin,end);

            }
            // else Midway is median
            // Order is ( Start, Midway, Finish)
            else
            {
                return reorder(vec, middle,begin,end);
            }
        }
        // Start < Finish & Start >  Midway
        // start is median
        // order is ( Midway, Start, Finish )
        else
        {
            MapObject * temp1 = first;
            vec[0] = middle;
            vec[begin + ((end-begin) / 2)] = temp1;
            return reorder(vec, first,begin,end);

        }
    }
}
int ObjectGroup::reorder(QVector<MapObject *> &vec, MapObject *pivot, int begin, int end)
{
    int pivotLocation = begin;
    const QString checkString = pivot->name();
    QVector<MapObject *> newVec;
    newVec.append(pivot);
    for (int i = begin; i < end; i++)
    {
        if ( vec[i]->equals(pivot))
        {
            vec.remove(i);
            end--;
            break;
        }
    }

    for (int i = 0; i < begin; i++)
    {
        newVec.push_front(vec[i]);
    }
    for (int i = begin; i < end; i++){
       const QString readStr = vec[i]->name();
       if (QString::compare(checkString, readStr, Qt::CaseInsensitive ) >= 0 )
       {
           pivotLocation++;
           newVec.insert(begin,vec[i]);
       }
       else
       {
           newVec.append(vec[i]);
       }
    }
    for (int i = end; i < vec.size(); i++)
    {
        newVec.append(vec[i]);
    }
    vec.clear();
    vec = newVec;
    return pivotLocation;

}

Layer *ObjectGroup::mergedWith(Layer *other) const
{
    Q_ASSERT(canMergeWith(other));

    const ObjectGroup *og = static_cast<ObjectGroup*>(other);

    ObjectGroup *merged = static_cast<ObjectGroup*>(clone());
    foreach (const MapObject *mapObject, og->objects())
        merged->addObject(mapObject->clone());
    return merged;
}

/**
 * Returns a duplicate of this ObjectGroup.
 *
 * \sa Layer::clone()
 */
Layer *ObjectGroup::clone() const
{
    return initializeClone(new ObjectGroup(mName, mX, mY, mWidth, mHeight));
}

ObjectGroup *ObjectGroup::initializeClone(ObjectGroup *clone) const
{
    Layer::initializeClone(clone);
    foreach (const MapObject *object, mObjects)
        clone->addObject(object->clone());
    clone->setColor(mColor);
    clone->setDrawOrder(mDrawOrder);
    return clone;
}


QString Tiled::drawOrderToString(ObjectGroup::DrawOrder drawOrder)
{
    switch (drawOrder) {
    default:
    case ObjectGroup::UnknownOrder:
        return QLatin1String("unknown");
        break;
    case ObjectGroup::TopDownOrder:
        return QLatin1String("topdown");
        break;
    case ObjectGroup::IndexOrder:
        return QLatin1String("index");
        break;
    }
}

ObjectGroup::DrawOrder Tiled::drawOrderFromString(const QString &string)
{
    ObjectGroup::DrawOrder drawOrder = ObjectGroup::UnknownOrder;

    if (string == QLatin1String("topdown"))
        drawOrder = ObjectGroup::TopDownOrder;
    else if (string == QLatin1String("index"))
        drawOrder = ObjectGroup::IndexOrder;

    return drawOrder;
}
