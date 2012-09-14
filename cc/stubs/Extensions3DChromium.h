// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_STUBS_EXTENSIONS3DCHROMIUM_H_
#define CC_STUBS_EXTENSIONS3DCHROMIUM_H_

#include "Extensions3D.h"

// These enum names collides with gl2ext.h, so we just redefine them.
#ifdef GL_TEXTURE_EXTERNAL_OES
#undef GL_TEXTURE_EXTERNAL_OES
#endif
#ifdef GL_TEXTURE_USAGE_ANGLE
#undef GL_TEXTURE_USAGE_ANGLE
#endif
#ifdef GL_FRAMEBUFFER_ATTACHMENT_ANGLE
#undef GL_FRAMEBUFFER_ATTACHMENT_ANGLE
#endif

namespace cc {

class Extensions3DChromium {
public:
    enum {
        // GL_OES_EGL_image_external
        GL_TEXTURE_EXTERNAL_OES = 0x8D65,

        // GL_CHROMIUM_map_sub (enums inherited from GL_ARB_vertex_buffer_object)
        READ_ONLY = 0x88B8,
        WRITE_ONLY = 0x88B9,

        // GL_ANGLE_texture_usage
        GL_TEXTURE_USAGE_ANGLE = 0x93A2,
        GL_FRAMEBUFFER_ATTACHMENT_ANGLE = 0x93A3,

        // GL_EXT_texture_storage
        BGRA8_EXT = 0x93A1,

        // GL_EXT_occlusion_query_boolean
        ANY_SAMPLES_PASSED_EXT = 0x8C2F,
        ANY_SAMPLES_PASSED_CONSERVATIVE_EXT = 0x8D6A,
        CURRENT_QUERY_EXT = 0x8865,
        QUERY_RESULT_EXT = 0x8866,
        QUERY_RESULT_AVAILABLE_EXT = 0x8867,

        // GL_CHROMIUM_command_buffer_query
        COMMANDS_ISSUED_CHROMIUM = 0x84F2
    };
};

}

#endif  // CC_STUBS_EXTENSIONS3DCHROMIUM_H_
