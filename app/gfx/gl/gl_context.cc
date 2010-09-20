// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "app/gfx/gl/gl_context.h"
#include "app/gfx/gl/gl_bindings.h"
#include "app/gfx/gl/gl_implementation.h"
#include "base/logging.h"

namespace gfx {

bool GLContext::HasExtension(const char* name) {
  DCHECK(IsCurrent());

  std::string extensions(reinterpret_cast<const char*>(
      glGetString(GL_EXTENSIONS)));
  extensions += " ";

  std::string delimited_name(name);
  delimited_name += " ";

  return extensions.find(delimited_name) != std::string::npos;
}

bool GLContext::InitializeCommon() {
  if (!MakeCurrent()) {
    LOG(ERROR) << "MakeCurrent failed.";
    return false;
  }

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  if (glGetError() != GL_NO_ERROR) {
    LOG(ERROR) << "glClear failed.";
    return false;
  }

  return true;
}

}  // namespace gfx
