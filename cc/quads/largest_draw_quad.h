// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_LARGEST_DRAW_QUAD_H_
#define CC_QUADS_LARGEST_DRAW_QUAD_H_

#include <stddef.h>

#include "cc/cc_export.h"

namespace cc {

CC_EXPORT size_t LargestDrawQuadSize();
CC_EXPORT size_t LargestDrawQuadAlignment();

}  // namespace cc

#endif  // CC_QUADS_LARGEST_DRAW_QUAD_H_
