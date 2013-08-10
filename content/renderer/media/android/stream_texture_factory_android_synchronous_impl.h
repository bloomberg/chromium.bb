// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_ANDROID_SYNCHRONOUS_IMPL_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_ANDROID_SYNCHRONOUS_IMPL_H_

#include "content/renderer/media/android/stream_texture_factory_android.h"

namespace content {

// Factory for when using synchronous compositor in Android WebView.
class StreamTextureFactorySynchronousImpl : public StreamTextureFactory {
 public:
  StreamTextureFactorySynchronousImpl();
  virtual ~StreamTextureFactorySynchronousImpl();

  virtual StreamTextureProxy* CreateProxy() OVERRIDE;
  virtual void EstablishPeer(int32 stream_id, int player_id) OVERRIDE;
  virtual unsigned CreateStreamTexture(
      unsigned texture_target,
      unsigned* texture_id,
      gpu::Mailbox* texture_mailbox,
      unsigned* texture_mailbox_sync_point) OVERRIDE;
  virtual void DestroyStreamTexture(unsigned texture_id) OVERRIDE;
  virtual void SetStreamTextureSize(int32 texture_id,
                                    const gfx::Size& size) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(StreamTextureFactorySynchronousImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_ANDROID_SYNCHRONOUS_IMPL_H_
