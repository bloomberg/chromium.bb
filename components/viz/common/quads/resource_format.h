// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_RESOURCE_FORMAT_H_
#define COMPONENTS_VIZ_COMMON_QUADS_RESOURCE_FORMAT_H_

#include "base/logging.h"
#include "ui/gfx/buffer_types.h"

// TODO(prashant.n): Including third_party/khronos/GLES2/gl2.h causes
// redefinition errors as macros/functions defined in it conflict with
// macros/functions defined in ui/gl/gl_bindings.h. (http://crbug.com/512833).
typedef unsigned int GLenum;

namespace viz {

// Keep in sync with arrays below.
enum ResourceFormat {
  RGBA_8888,
  RGBA_4444,
  BGRA_8888,
  ALPHA_8,
  LUMINANCE_8,
  RGB_565,
  ETC1,
  RED_8,
  LUMINANCE_F16,
  RGBA_F16,
  RESOURCE_FORMAT_MAX = RGBA_F16,
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_RESOURCE_FORMAT_H_
