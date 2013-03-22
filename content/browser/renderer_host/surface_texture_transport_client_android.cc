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
#include "content/common/android/surface_texture_bridge.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "webkit/compositor_bindings/web_compositor_support_impl.h"

namespace {

static const uint32 kGLTextureExternalOES = 0x8D65;

} // anonymous namespace

namespace content {

SurfaceTextureTransportClient::SurfaceTextureTransportClient()
    : window_(NULL),
      texture_id_(0),
      surface_id_(0) {
}

SurfaceTextureTransportClient::~SurfaceTextureTransportClient() {
  if (surface_id_) {
    GpuSurfaceTracker::Get()->SetNativeWidget(
        surface_id_, gfx::kNullAcceleratedWidget);
  }
  if (window_)
    ANativeWindow_release(window_);
}

scoped_refptr<cc::Layer> SurfaceTextureTransportClient::Initialize() {
  // Use a SurfaceTexture to stream frames to the UI thread.
  video_layer_ = cc::VideoLayer::Create(this);

  surface_texture_ = new SurfaceTextureBridge(0);
  surface_texture_->SetFrameAvailableCallback(
      base::Bind(
          &SurfaceTextureTransportClient::OnSurfaceTextureFrameAvailable,
          base::Unretained(this)));
  surface_texture_->DetachFromGLContext();
  return video_layer_.get();
}

gfx::GLSurfaceHandle
SurfaceTextureTransportClient::GetCompositingSurface(int surface_id) {
  DCHECK(surface_id);
  surface_id_ = surface_id;

  if (!window_)
    window_ = surface_texture_->CreateSurface();

  GpuSurfaceTracker::Get()->SetNativeWidget(surface_id, window_);
  return gfx::GLSurfaceHandle(gfx::kNullPluginWindow, gfx::NATIVE_DIRECT);
}

void SurfaceTextureTransportClient::SetSize(const gfx::Size& size) {
  surface_texture_->SetDefaultBufferSize(size.width(), size.height());
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
  video_layer_->SetNeedsDisplay();
}

} // namespace content
