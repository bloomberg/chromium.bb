// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines constants and functions for the Pepper gles2 command
// buffers that fall outside the scope of OpenGL ES 2.0

#ifndef GPU_GLES2_GLES2_COMMAND_BUFFER_H_
#define GPU_GLES2_GLES2_COMMAND_BUFFER_H_

// constants for CommandBufferEnableCHROMIUM command.
#define PEPPER3D_ALLOW_BUFFERS_ON_MULTIPLE_TARGETS \
    "pepper3d_allow_buffers_on_multiple_targets"
#define PEPPER3D_SUPPORT_FIXED_ATTRIBS \
    "pepper3d_support_fixed_attribs"
// TODO(gman): remove this
#define PEPPER3D_SKIP_GLSL_TRANSLATION \
    "pepper3d_skip_glsl_translation"

// TODO(gman): move this somewhere else.
#ifndef GL_READ_ONLY
#define GL_READ_ONLY 0x88B8
#endif
#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY 0x88B9
#endif
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif
#ifndef GL_HALF_FLOAT_OES
#define GL_HALF_FLOAT_OES 0x8D61
#endif

#endif  // GPU_GLES2_GLES2_COMMAND_BUFFER_H_




