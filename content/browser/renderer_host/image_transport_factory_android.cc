// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/image_transport_factory_android.h"

#include "base/memory/singleton.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_impl.h"

namespace content {

namespace {

static ImageTransportFactoryAndroid* g_factory = NULL;

class DirectGLImageTransportFactory : public ImageTransportFactoryAndroid {
 public:
  DirectGLImageTransportFactory();
  virtual ~DirectGLImageTransportFactory();

  virtual gfx::GLSurfaceHandle CreateSharedSurfaceHandle() OVERRIDE {
    return gfx::GLSurfaceHandle();
  }
  virtual void DestroySharedSurfaceHandle(
      const gfx::GLSurfaceHandle& handle) OVERRIDE {}
  virtual uint32_t InsertSyncPoint() OVERRIDE { return 0; }
  virtual WebKit::WebGraphicsContext3D* GetContext3D() OVERRIDE {
    return context_.get();
  }
  virtual GLHelper* GetGLHelper() OVERRIDE { return NULL; }

 private:
  scoped_ptr<webkit::gpu::WebGraphicsContext3DInProcessImpl> context_;

  DISALLOW_COPY_AND_ASSIGN(DirectGLImageTransportFactory);
};

DirectGLImageTransportFactory::DirectGLImageTransportFactory() {
  WebKit::WebGraphicsContext3D::Attributes attrs;
  attrs.shareResources = false;
  attrs.noAutomaticFlushes = true;
  context_.reset(
      webkit::gpu::WebGraphicsContext3DInProcessImpl::CreateForWindow(
          attrs,
          NULL,
          NULL));
}

DirectGLImageTransportFactory::~DirectGLImageTransportFactory() {
}

class CmdBufferImageTransportFactory : public ImageTransportFactoryAndroid {
 public:
  CmdBufferImageTransportFactory();
  virtual ~CmdBufferImageTransportFactory();

  virtual gfx::GLSurfaceHandle CreateSharedSurfaceHandle() OVERRIDE;
  virtual void DestroySharedSurfaceHandle(
      const gfx::GLSurfaceHandle& handle) OVERRIDE;
  virtual uint32_t InsertSyncPoint() OVERRIDE;
  virtual WebKit::WebGraphicsContext3D* GetContext3D() OVERRIDE {
    return context_.get();
  }
  virtual GLHelper* GetGLHelper() OVERRIDE;

 private:
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context_;
  scoped_ptr<GLHelper> gl_helper_;

  DISALLOW_COPY_AND_ASSIGN(CmdBufferImageTransportFactory);
};

CmdBufferImageTransportFactory::CmdBufferImageTransportFactory() {
  WebKit::WebGraphicsContext3D::Attributes attrs;
  attrs.shareResources = true;
  GpuChannelHostFactory* factory = BrowserGpuChannelHostFactory::instance();
  GURL url("chrome://gpu/ImageTransportFactoryAndroid");
  base::WeakPtr<WebGraphicsContext3DSwapBuffersClient> swap_client;
  context_.reset(new WebGraphicsContext3DCommandBufferImpl(0, // offscreen
                                                           url,
                                                           factory,
                                                           swap_client));
  context_->Initialize(
      attrs,
      false,
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE);
}

CmdBufferImageTransportFactory::~CmdBufferImageTransportFactory() {
}

gfx::GLSurfaceHandle
CmdBufferImageTransportFactory::CreateSharedSurfaceHandle() {
  if (!context_->makeContextCurrent()) {
    NOTREACHED() << "Failed to make shared graphics context current";
    return gfx::GLSurfaceHandle();
  }

  gfx::GLSurfaceHandle handle = gfx::GLSurfaceHandle(
      gfx::kNullPluginWindow, true);
  handle.parent_gpu_process_id = context_->GetGPUProcessID();
  handle.parent_client_id = context_->GetChannelID();
  handle.parent_context_id = context_->GetContextID();
  handle.parent_texture_id[0] = context_->createTexture();
  handle.parent_texture_id[1] = context_->createTexture();
  handle.sync_point = context_->insertSyncPoint();
  context_->flush();
  return handle;
}

void CmdBufferImageTransportFactory::DestroySharedSurfaceHandle(
    const gfx::GLSurfaceHandle& handle) {
  if (!context_->makeContextCurrent()) {
    NOTREACHED() << "Failed to make shared graphics context current";
    return;
  }

  context_->deleteTexture(handle.parent_texture_id[0]);
  context_->deleteTexture(handle.parent_texture_id[1]);
  context_->finish();
}

uint32_t CmdBufferImageTransportFactory::InsertSyncPoint() {
  return context_->insertSyncPoint();
}

GLHelper* CmdBufferImageTransportFactory::GetGLHelper() {
  if (!gl_helper_.get())
    gl_helper_.reset(new GLHelper(GetContext3D(), NULL));

  return gl_helper_.get();
}

}  // anonymous namespace

// static
ImageTransportFactoryAndroid* ImageTransportFactoryAndroid::GetInstance() {
  if (!g_factory) {
    if (CompositorImpl::UsesDirectGL())
      g_factory = new DirectGLImageTransportFactory();
    else
      g_factory = new CmdBufferImageTransportFactory();
  }

  return g_factory;
}

ImageTransportFactoryAndroid::ImageTransportFactoryAndroid() {
}

ImageTransportFactoryAndroid::~ImageTransportFactoryAndroid() {
}

} // namespace content
