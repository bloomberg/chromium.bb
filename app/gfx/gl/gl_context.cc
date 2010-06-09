// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/gl/gl_context.h"
#include "app/gfx/gl/gl_bindings.h"
#include "app/gfx/gl/gl_implementation.h"
#include "base/logging.h"

namespace gfx {

bool GLContext::InitializeCommon() {
  if (!MakeCurrent())
    return false;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  if (glGetError() != GL_NO_ERROR)
    return false;

  return true;
}
}  // namespace gfx
