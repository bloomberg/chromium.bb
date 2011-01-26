// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_RENDERER_PEPPER_PLATFORM_CONTEXT_3D_IMPL_H_
#define CHROME_RENDERER_PEPPER_PLATFORM_CONTEXT_3D_IMPL_H_

#include "webkit/plugins/ppapi/plugin_delegate.h"

#ifdef ENABLE_GPU

namespace ggl {

class Context;

}  // namespace ggl;

class PlatformContext3DImpl
    : public webkit::ppapi::PluginDelegate::PlatformContext3D {
 public:
  explicit PlatformContext3DImpl(ggl::Context* parent_context);
  virtual ~PlatformContext3DImpl();

  virtual bool Init();
  virtual bool SwapBuffers();
  virtual unsigned GetError();
  virtual void SetSwapBuffersCallback(Callback0::Type* callback);
  virtual unsigned GetBackingTextureId();
  virtual gpu::gles2::GLES2Implementation* GetGLES2Implementation();

 private:
  ggl::Context* parent_context_;
  ggl::Context* context_;
};

#endif  // ENABLE_GPU

#endif  // CHROME_RENDERER_PEPPER_PLATFORM_CONTEXT_3D_IMPL_H_
