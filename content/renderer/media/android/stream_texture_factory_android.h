// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_ANDROID_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_ANDROID_H_

#include "base/memory/scoped_ptr.h"
#include "cc/layers/video_frame_provider.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/gfx/size.h"

namespace blink {
class WebGraphicsContext3D;
}

namespace content {

// The proxy class for the gpu thread to notify the compositor thread
// when a new video frame is available.
class StreamTextureProxy {
 public:
  virtual ~StreamTextureProxy() {}

  // Initialize and bind to the current thread, which becomes the thread that
  // a connected client will receive callbacks on.
  virtual void BindToCurrentThread(int32 stream_id) = 0;

  virtual bool IsBoundToThread() = 0;

  // Setting the target for callback when a frame is available. This function
  // could be called on both the main thread and the compositor thread.
  virtual void SetClient(cc::VideoFrameProvider::Client* client) = 0;

  // Causes this instance to be deleted on the thread it is bound to.
  virtual void Release() = 0;

  struct Deleter {
    inline void operator()(StreamTextureProxy* ptr) const { ptr->Release(); }
  };
};

typedef scoped_ptr<StreamTextureProxy, StreamTextureProxy::Deleter>
    ScopedStreamTextureProxy;

// Factory class for managing stream textures.
class StreamTextureFactory {
 public:
  virtual ~StreamTextureFactory() {}

  // Create the StreamTextureProxy object.
  virtual StreamTextureProxy* CreateProxy() = 0;

  // Send an IPC message to the browser process to request a java surface
  // object for the given stream_id. After the the surface is created,
  // it will be passed back to the WebMediaPlayerAndroid object identified by
  // the player_id.
  virtual void EstablishPeer(int32 stream_id, int player_id) = 0;

  // Create the streamTexture and return the stream Id and create a client-side
  // texture id to refer to the streamTexture. The texture id is produced into
  // a mailbox so it can be used to ship in a VideoFrame, with a sync point for
  // when the mailbox can be accessed.
  virtual unsigned CreateStreamTexture(
      unsigned texture_target,
      unsigned* texture_id,
      gpu::Mailbox* texture_mailbox,
      unsigned* texture_mailbox_sync_point) = 0;

  // Destroy the streamTexture for the given texture id, as well as the
  // client side texture.
  virtual void DestroyStreamTexture(unsigned texture_id) = 0;

  // Set the streamTexture size for the given stream Id.
  virtual void SetStreamTextureSize(int32 texture_id,
                                    const gfx::Size& size) = 0;

  virtual blink::WebGraphicsContext3D* Context3d() = 0;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_ANDROID_H_
