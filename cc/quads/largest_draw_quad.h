// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_LARGEST_DRAW_QUAD_H_
#define CC_QUADS_LARGEST_DRAW_QUAD_H_

namespace cc {
class StreamVideoDrawQuad;
class RenderPassDrawQuad;

#if defined(ARCH_CPU_64_BITS)
typedef RenderPassDrawQuad kLargestDrawQuad;
#else
typedef StreamVideoDrawQuad kLargestDrawQuad;
#endif

}  // namespace cc

#endif  // CC_QUADS_LARGEST_DRAW_QUAD_H_
