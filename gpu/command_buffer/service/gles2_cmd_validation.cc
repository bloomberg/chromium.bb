// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains various validation functions for the GLES2 service.

// NOTE: We explicitly do NOT include gles2_cmd_validation.h because the
//     gl2.h included in here must NOT be the native one. This is because
//     some of the GLenum definitions exist only in GLES2 and not in Desktop
//     GL.
#include <GLES2/gl2types.h>

namespace gpu {
namespace gles2 {

#include "gpu/command_buffer/service/gles2_cmd_validation_implementation_autogen.h"

}  // namespace gles2
}  // namespace gpu



