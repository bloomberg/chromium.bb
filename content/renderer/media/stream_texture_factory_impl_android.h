// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_TEXTURE_IMPL_ANDROID_H_
#define CONTENT_RENDERER_MEDIA_STREAM_TEXTURE_IMPL_ANDROID_H_

#include "base/memory/scoped_ptr.h"
#include "content/renderer/gpu/stream_texture_host_android.h"
#include "webkit/media/android/stream_texture_factory_android.h"

namespace WebKit {
class WebGraphicsContext3D;
}

namespace content {
class GpuChannelHost;

// The acutal implementation of StreamTextureFactory class.
class StreamTextureFactoryImpl
    : public webkit_media::StreamTextureFactory {
 public:
  StreamTextureFactoryImpl(WebKit::WebGraphicsContext3D* context,
                           GpuChannelHost* channel,
                           int view_id);
  virtual ~StreamTextureFactoryImpl();

  // webkit_media::StreamTextureFactory implementation:
  virtual webkit_media::StreamTextureProxy* CreateProxy() OVERRIDE;

  virtual void EstablishPeer(int stream_id, int player_id) OVERRIDE;

  virtual unsigned CreateStreamTexture(unsigned* texture_id) OVERRIDE;
  virtual void DestroyStreamTexture(unsigned texture_id) OVERRIDE;

 private:
  WebKit::WebGraphicsContext3D* context_;
  scoped_refptr<GpuChannelHost> channel_;
  int view_id_;
  DISALLOW_COPY_AND_ASSIGN(StreamTextureFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_STREAM_TEXTURE_IMPL_ANDROID_H_
