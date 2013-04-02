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
#include "third_party/khronos/GLES2/gl2.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_impl.h"

namespace content {

namespace {

static ImageTransportFactoryAndroid* g_factory = NULL;

class DirectGLImageTransportFactory : public ImageTransportFactoryAndroid {
 public:
  DirectGLImageTransportFactory();
  virtual ~DirectGLImageTransportFactory();

  virtual uint32_t InsertSyncPoint() OVERRIDE { return 0; }
  virtual void WaitSyncPoint(uint32_t sync_point) OVERRIDE {}
  virtual uint32_t CreateTexture() OVERRIDE {
    return context_->createTexture();
  }
  virtual void DeleteTexture(uint32_t id) OVERRIDE {
    context_->deleteTexture(id);
  }
  virtual void AcquireTexture(
      uint32 texture_id, const signed char* mailbox_name) OVERRIDE {}
  virtual void ReleaseTexture(
      uint32 texture_id, const signed char* mailbox_name) OVERRIDE {}
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

  virtual uint32_t InsertSyncPoint() OVERRIDE;
  virtual void WaitSyncPoint(uint32_t sync_point) OVERRIDE;
  virtual uint32_t CreateTexture() OVERRIDE;
  virtual void DeleteTexture(uint32_t id) OVERRIDE;
  virtual void AcquireTexture(
      uint32 texture_id, const signed char* mailbox_name) OVERRIDE;
  virtual void ReleaseTexture(
      uint32 texture_id, const signed char* mailbox_name) OVERRIDE;
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

uint32_t CmdBufferImageTransportFactory::InsertSyncPoint() {
  if (!context_->makeContextCurrent()) {
    LOG(ERROR) << "Failed to make helper context current.";
    return 0;
  }
  return context_->insertSyncPoint();
}

void CmdBufferImageTransportFactory::WaitSyncPoint(uint32_t sync_point) {
  if (!context_->makeContextCurrent()) {
    LOG(ERROR) << "Failed to make helper context current.";
    return;
  }
  context_->waitSyncPoint(sync_point);
}

uint32_t CmdBufferImageTransportFactory::CreateTexture() {
  if (!context_->makeContextCurrent()) {
    LOG(ERROR) << "Failed to make helper context current.";
    return false;
  }
  return context_->createTexture();
}

void CmdBufferImageTransportFactory::DeleteTexture(uint32_t id) {
  if (!context_->makeContextCurrent()) {
    LOG(ERROR) << "Failed to make helper context current.";
    return;
  }
  context_->deleteTexture(id);
}

void CmdBufferImageTransportFactory::AcquireTexture(
    uint32 texture_id, const signed char* mailbox_name) {
  if (!context_->makeContextCurrent()) {
    LOG(ERROR) << "Failed to make helper context current.";
    return;
  }
  context_->bindTexture(GL_TEXTURE_2D, texture_id);
  context_->consumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox_name);
  context_->flush();
}

void CmdBufferImageTransportFactory::ReleaseTexture(
    uint32 texture_id, const signed char* mailbox_name) {
  if (!context_->makeContextCurrent()) {
    LOG(ERROR) << "Failed to make helper context current.";
    return;
  }
  context_->bindTexture(GL_TEXTURE_2D, texture_id);
  context_->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox_name);
}

GLHelper* CmdBufferImageTransportFactory::GetGLHelper() {
  if (!gl_helper_.get())
    gl_helper_.reset(new GLHelper(context_.get()));

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
