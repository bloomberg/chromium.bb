// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/graphics_context.h"

#include "base/memory/scoped_ptr.h"
#include "content/browser/android/draw_delegate_impl.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "ui/gfx/native_widget_types.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebGraphicsContext3D.h"

#include <android/native_window_jni.h>

using content::BrowserGpuChannelHostFactory;

namespace {

// GraphicsContext implementation using a gpu command buffer.
class CmdBufferGraphicsContext : public content::GraphicsContext {
 public:
  CmdBufferGraphicsContext(WebGraphicsContext3DCommandBufferImpl* context,
                           int surface_id,
                           ANativeWindow* window,
                           int texture_id1,
                           int texture_id2)
      : context_(context),
        surface_id_(surface_id),
        window_(window) {
    texture_id_[0] = texture_id1;
    texture_id_[1] = texture_id2;
  }

  virtual ~CmdBufferGraphicsContext() {
    context_->makeContextCurrent();
    context_->deleteTexture(texture_id_[0]);
    context_->deleteTexture(texture_id_[1]);
    context_->finish();
    GpuSurfaceTracker* tracker = GpuSurfaceTracker::Get();
    tracker->RemoveSurface(surface_id_);
    ANativeWindow_release(window_);
  }

  virtual WebKit::WebGraphicsContext3D* GetContext3D() {
    return context_.get();
  }
  virtual uint32 InsertSyncPoint() {
    return context_->insertSyncPoint();
  }

 private:
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context_;
  int surface_id_;
  ANativeWindow* window_;
  int texture_id_[2];
};

}  // anonymous namespace

namespace content {

// static
GraphicsContext* GraphicsContext::CreateForUI(
    ANativeWindow* window) {
  DCHECK(window);
  GpuSurfaceTracker* tracker = GpuSurfaceTracker::Get();

  ANativeWindow_acquire(window);
  int surface_id = tracker->AddSurfaceForNativeWidget(window);

  tracker->SetSurfaceHandle(
      surface_id,
      gfx::GLSurfaceHandle(gfx::kDummyPluginWindow, false));

  WebKit::WebGraphicsContext3D::Attributes attrs;
  attrs.shareResources = true;
  GpuChannelHostFactory* factory = BrowserGpuChannelHostFactory::instance();
  GURL url("chrome://gpu/GpuProcessTransportHelper::CreateContext");
  base::WeakPtr<WebGraphicsContext3DSwapBuffersClient> swap_client;
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context(
      new WebGraphicsContext3DCommandBufferImpl(
          surface_id,
          url,
          factory,
          swap_client));
  if (!context->Initialize(
      attrs,
      false,
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE)) {
    return NULL;
  }

  context->makeContextCurrent();

  gfx::GLSurfaceHandle handle = gfx::GLSurfaceHandle(
      gfx::kNullPluginWindow, true);
  handle.parent_gpu_process_id = context->GetGPUProcessID();
  handle.parent_client_id = context->GetChannelID();
  handle.parent_context_id = context->GetContextID();
  handle.parent_texture_id[0] = context->createTexture();
  handle.parent_texture_id[1] = context->createTexture();
  handle.sync_point = context->insertSyncPoint();

  DrawDelegateImpl::GetInstance()->SetDrawSurface(handle);

  return new CmdBufferGraphicsContext(
      context.release(), surface_id, window,
      handle.parent_texture_id[0],
      handle.parent_texture_id[1]);
}

}  // namespace content
