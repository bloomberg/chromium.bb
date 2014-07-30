// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_BSP_WALK_ACTION_H_
#define CC_OUTPUT_BSP_WALK_ACTION_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "cc/output/direct_renderer.h"
#include "cc/quads/draw_polygon.h"

namespace cc {

class CC_EXPORT BspWalkAction {
 public:
  virtual void operator()(DrawPolygon* item) = 0;
};

// The BspTree class takes ownership of all the DrawPolygons returned in list_
// so the BspTree must be preserved while elements in that vector are in use.
class CC_EXPORT BspWalkActionToVector : public BspWalkAction {
 public:
  explicit BspWalkActionToVector(std::vector<DrawPolygon*>* in_list);
  virtual void operator()(DrawPolygon* item) OVERRIDE;

 private:
  std::vector<DrawPolygon*>* list_;
};

}  // namespace cc

#endif  // CC_OUTPUT_BSP_WALK_ACTION_H_
