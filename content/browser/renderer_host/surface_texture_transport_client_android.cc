// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/surface_texture_transport_client_android.h"

#include <android/native_window_jni.h>

#include "base/bind.h"
#include "cc/layers/video_layer.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/image_transport_factory_android.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "ui/gl/android/surface_texture_bridge.h"
#include "webkit/compositor_bindings/web_compositor_support_impl.h"

namespace content {

namespace {

static const uint32 kGLTextureExternalOES = 0x8D65;

class SurfaceRefAndroid : public GpuSurfaceTracker::SurfaceRef {
 public:
  SurfaceRefAndroid(
      const scoped_refptr<gfx::SurfaceTextureBridge>& surface,
      ANativeWindow* window)
      : surface_(surface),
        window_(window) {
    ANativeWindow_acquire(window_);
  }

 private:
  virtual ~SurfaceRefAndroid() {
    DCHECK(window_);
    ANativeWindow_release(window_);
  }

  scoped_refptr<gfx::SurfaceTextureBridge> surface_;
  ANativeWindow* window_;
};

} // anonymous namespace

SurfaceTextureTransportClient::SurfaceTextureTransportClient()
    : window_(NULL),
      texture_id_(0),
      surface_id_(0),
      weak_factory_(this) {
}

SurfaceTextureTransportClient::~SurfaceTextureTransportClient() {
}

scoped_refptr<cc::Layer> SurfaceTextureTransportClient::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Use a SurfaceTexture to stream frames to the UI thread.
  video_layer_ = cc::VideoLayer::Create(this);

  surface_texture_ = new gfx::SurfaceTextureBridge(0);
  surface_texture_->SetFrameAvailableCallback(
    base::Bind(
        &SurfaceTextureTransportClient::OnSurfaceTextureFrameAvailable,
        weak_factory_.GetWeakPtr()));
  surface_texture_->DetachFromGLContext();
  return video_layer_.get();
}

gfx::GLSurfaceHandle
SurfaceTextureTransportClient::GetCompositingSurface(int surface_id) {
  DCHECK(surface_id);
  surface_id_ = surface_id;

  if (!window_) {
    window_ = surface_texture_->CreateSurface();

    GpuSurfaceTracker::Get()->SetNativeWidget(
        surface_id, window_, new SurfaceRefAndroid(surface_texture_, window_));
    // SurfaceRefAndroid took ownership (and an extra ref to) window_.
    ANativeWindow_release(window_);
  }

  return gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::NATIVE_DIRECT);
}

void SurfaceTextureTransportClient::SetSize(const gfx::Size& size) {
  if (size.width() > 0 && size.height() > 0) {
    surface_texture_->SetDefaultBufferSize(size.width(), size.height());
  }
  video_layer_->SetBounds(size);
  video_frame_ = NULL;
}

scoped_refptr<media::VideoFrame> SurfaceTextureTransportClient::
    GetCurrentFrame() {
  if (!texture_id_) {
    WebKit::WebGraphicsContext3D* context =
        ImageTransportFactoryAndroid::GetInstance()->GetContext3D();
    context->makeContextCurrent();
    texture_id_ = context->createTexture();
    surface_texture_->AttachToGLContext(texture_id_);
  }
  if (!video_frame_) {
    const gfx::Size size = video_layer_->bounds();
    video_frame_ = media::VideoFrame::WrapNativeTexture(
        texture_id_, kGLTextureExternalOES,
        size,
        gfx::Rect(gfx::Point(), size),
        size,
        base::TimeDelta(),
        media::VideoFrame::ReadPixelsCB(),
        base::Closure());
  }
  surface_texture_->UpdateTexImage();

  return video_frame_;
}

void SurfaceTextureTransportClient::PutCurrentFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
}

void SurfaceTextureTransportClient::OnSurfaceTextureFrameAvailable() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  video_layer_->SetNeedsDisplay();
}

} // namespace content
