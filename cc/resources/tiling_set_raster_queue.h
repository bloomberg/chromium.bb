// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILING_SET_RASTER_QUEUE_H_
#define CC_RESOURCES_TILING_SET_RASTER_QUEUE_H_

namespace cc {
class Tile;

// TODO(vmpstr): Remove this, since RasterTilePriorityQueue knows the type
// directly.
class TilingSetRasterQueue {
 public:
  virtual ~TilingSetRasterQueue() {}

  virtual Tile* Top() = 0;
  virtual const Tile* Top() const = 0;
  virtual void Pop() = 0;
  virtual bool IsEmpty() const = 0;
};

}  // namespace cc

#endif  // CC_RESOURCES_TILING_SET_RASTER_QUEUE_H_
