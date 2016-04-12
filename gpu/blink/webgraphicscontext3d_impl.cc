// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/blink/webgraphicscontext3d_impl.h"

#include <stdint.h>

#include "base/atomicops.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sys_info.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"

#include "third_party/khronos/GLES2/gl2.h"
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include "third_party/khronos/GLES2/gl2ext.h"

namespace gpu_blink {

WebGraphicsContext3DImpl::WebGraphicsContext3DImpl()
    : initialized_(false),
      initialize_failed_(false),
      context_lost_callback_(0),
      gl_(NULL) {}

WebGraphicsContext3DImpl::~WebGraphicsContext3DImpl() {

}

void WebGraphicsContext3DImpl::setContextLostCallback(
    WebGraphicsContext3D::WebGraphicsContextLostCallback* cb) {
  context_lost_callback_ = cb;
}

}  // namespace gpu_blink
