// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ganesh_surface_provider.h"

#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrContextOptions.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "ui/gfx/geometry/size.h"

namespace vr {

GaneshSurfaceProvider::GaneshSurfaceProvider() {
  gr_context_ = GrContext::MakeGL(nullptr);
  DCHECK(gr_context_.get());
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &main_fbo_);
}

GaneshSurfaceProvider::~GaneshSurfaceProvider() = default;

sk_sp<SkSurface> GaneshSurfaceProvider::MakeSurface(const gfx::Size& size) {
  return SkSurface::MakeRenderTarget(
      gr_context_.get(), SkBudgeted::kNo,
      SkImageInfo::MakeN32Premul(size.width(), size.height()), 0,
      kTopLeft_GrSurfaceOrigin, nullptr);
}

GLuint GaneshSurfaceProvider::FlushSurface(SkSurface* surface,
                                           GLuint reuse_texture_id) {
  surface->getCanvas()->flush();
  GrBackendObject backend_object =
      surface->getTextureHandle(SkSurface::kFlushRead_BackendHandleAccess);
  DCHECK_NE(backend_object, 0);
  GLuint texture_id =
      skia::GrBackendObjectToGrGLTextureInfo(backend_object)->fID;
  DCHECK_NE(texture_id, 0u);
  surface->getCanvas()->getGrContext()->resetContext();
  glBindFramebufferEXT(GL_FRAMEBUFFER, main_fbo_);

  return texture_id;
}

}  // namespace vr
