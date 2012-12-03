// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_SURFACE_TEXTURE_TRANSPORT_CLIENT_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_SURFACE_TEXTURE_TRANSPORT_CLIENT_ANDROID_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVideoFrameProvider.h"
#include "ui/gfx/native_widget_types.h"

struct ANativeWindow;

namespace cc {
class Layer;
class VideoLayer;
}

namespace content {
class SurfaceTextureBridge;

class SurfaceTextureTransportClient : public WebKit::WebVideoFrameProvider {
 public:
  SurfaceTextureTransportClient();
  virtual ~SurfaceTextureTransportClient();

  scoped_refptr<cc::Layer> Initialize();
  gfx::GLSurfaceHandle GetCompositingSurface(int surface_id);
  void SetSize(const gfx::Size& size);

  // WebKit::WebVideoFrameProvider implementation.
  virtual void setVideoFrameProviderClient(Client*) OVERRIDE {}
  virtual WebKit::WebVideoFrame* getCurrentFrame() OVERRIDE;
  virtual void putCurrentFrame(WebKit::WebVideoFrame* frame) OVERRIDE;

 private:
  void OnSurfaceTextureFrameAvailable();

  scoped_refptr<cc::VideoLayer> video_layer_;
  scoped_refptr<SurfaceTextureBridge> surface_texture_;
  ANativeWindow* window_;
  scoped_ptr<WebKit::WebVideoFrame> video_frame_;
  uint32 texture_id_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceTextureTransportClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_SURFACE_TEXTURE_TRANSPORT_CLIENT_ANDROID_H_
