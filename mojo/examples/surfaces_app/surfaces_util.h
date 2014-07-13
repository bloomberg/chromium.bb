// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_SURFACES_APP_SURFACES_UTIL_H_
#define MOJO_EXAMPLES_SURFACES_APP_SURFACES_UTIL_H_

namespace cc {
class RenderPass;
}

namespace gfx {
class Transform;
class Size;
}

namespace mojo {
namespace examples {

void CreateAndAppendSimpleSharedQuadState(cc::RenderPass* render_pass,
                                          const gfx::Transform& transform,
                                          const gfx::Size& size);

}  // namespace mojo
}  // namespace examples

#endif  // MOJO_EXAMPLES_SURFACES_APP_SURFACES_UTIL_H_
