// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TEST_COMMON_H_
#define CC_TEST_LAYER_TEST_COMMON_H_

namespace gfx { class Rect; }

namespace cc {
class QuadList;

class LayerTestCommon {
 public:
  static const char* quad_string;

  static void VerifyQuadsExactlyCoverRect(const cc::QuadList& quads,
                                          gfx::Rect rect);
};

}  // namespace cc

#endif  // CC_TEST_LAYER_TEST_COMMON_H_
