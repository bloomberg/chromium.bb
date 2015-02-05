// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SCROLL_BLOCKS_ON_H_
#define CC_LAYERS_SCROLL_BLOCKS_ON_H_

enum ScrollBlocksOn {
  ScrollBlocksOnNone = 0x0,
  ScrollBlocksOnStartTouch = 0x1,
  ScrollBlocksOnWheelEvent = 0x2,
  ScrollBlocksOnScrollEvent = 0x4,
  ScrollBlocksOnMax = ScrollBlocksOnStartTouch | ScrollBlocksOnWheelEvent |
                      ScrollBlocksOnScrollEvent
};

inline ScrollBlocksOn operator|(ScrollBlocksOn a, ScrollBlocksOn b) {
  return ScrollBlocksOn(static_cast<int>(a) | static_cast<int>(b));
}

inline ScrollBlocksOn& operator|=(ScrollBlocksOn& a, ScrollBlocksOn b) {
  return a = a | b;
}

#endif  // CC_LAYERS_SCROLL_BLOCKS_ON_H_
