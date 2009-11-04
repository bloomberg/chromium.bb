// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_GFX_PATH_H_
#define APP_GFX_PATH_H_

#include "app/gfx/native_widget_types.h"
#include "base/basictypes.h"

#include "third_party/skia/include/core/SkPath.h"

namespace gfx {

class Path : public SkPath {
 public:
  // Used by Path(Point,size_t) constructor.
  struct Point {
    int x;
    int y;
  };

  Path() : SkPath() { moveTo(0, 0); }

  // Creates a path populated with the specified points.
  Path(const Point* points, size_t count);

#if defined(OS_WIN) || defined(USE_X11)
  // Creates a NativeRegion from the path. The caller is responsible for freeing
  // resources used by this region. This only supports polygon paths.
  NativeRegion CreateNativeRegion() const;

  // Returns the intersection of the two regions. The caller owns the returned
  // object.
  static gfx::NativeRegion IntersectRegions(gfx::NativeRegion r1,
                                            gfx::NativeRegion r2);

  // Returns the union of the two regions. The caller owns the returned object.
  static gfx::NativeRegion CombineRegions(gfx::NativeRegion r1,
                                          gfx::NativeRegion r2);

  // Returns the difference of the two regions. The caller owns the returned
  // object.
  static gfx::NativeRegion SubtractRegion(gfx::NativeRegion r1,
                                          gfx::NativeRegion r2);
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(Path);
};

}

#endif  // APP_GFX_PATH_H_
