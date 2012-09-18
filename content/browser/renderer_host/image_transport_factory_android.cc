// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/image_transport_factory_android.h"

#include "base/memory/singleton.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"

using content::BrowserGpuChannelHostFactory;

namespace content {

// static
ImageTransportFactoryAndroid* ImageTransportFactoryAndroid::GetInstance() {
  return Singleton<ImageTransportFactoryAndroid>::get();
}

ImageTransportFactoryAndroid::ImageTransportFactoryAndroid() {
  WebKit::WebGraphicsContext3D::Attributes attrs;
  attrs.shareResources = true;
  GpuChannelHostFactory* factory = BrowserGpuChannelHostFactory::instance();
  GURL url("chrome://gpu/ImageTransportFactoryAndroid");
  base::WeakPtr<WebGraphicsContext3DSwapBuffersClient> swap_client;
  context_.reset(
      new WebGraphicsContext3DCommandBufferImpl(
          0, // offscreen
          url,
          factory,
          swap_client));
  context_->Initialize(
      attrs,
      false,
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE);
}

ImageTransportFactoryAndroid::~ImageTransportFactoryAndroid() {
}

gfx::GLSurfaceHandle ImageTransportFactoryAndroid::CreateSharedSurfaceHandle() {
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

void ImageTransportFactoryAndroid::DestroySharedSurfaceHandle(
    const gfx::GLSurfaceHandle& handle) {
  if (!context_->makeContextCurrent()) {
    NOTREACHED() << "Failed to make shared graphics context current";
    return;
  }

  context_->deleteTexture(handle.parent_texture_id[0]);
  context_->deleteTexture(handle.parent_texture_id[1]);
  context_->finish();
}

uint32_t ImageTransportFactoryAndroid::InsertSyncPoint() {
  return context_->insertSyncPoint();
}

}  // namespace content
