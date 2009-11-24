// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the GLES2 command buffer commands.

#ifndef GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_H
#define GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_H

// This is here because service side code must include the system's version of
// the GL headers where as client side code includes the Chrome version.
#ifdef GLES2_GPU_SERVICE
#include <GL/glew.h>
#if defined(OS_WIN)
#include <GL/wglew.h>
#endif
#else
#include <GLES2/gl2types.h>
#endif

#include "base/basictypes.h"
#include "gpu/command_buffer/common/types.h"
#include "gpu/command_buffer/common/bitfield_helpers.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/common/gles2_cmd_ids.h"

namespace command_buffer {
namespace gles2 {

#include "gpu/command_buffer/common/gles2_cmd_format_autogen.h"

}  // namespace gles2
}  // namespace command_buffer

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_FORMAT_H

