// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_ANDROID_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_ANDROID_H_

#include "base/memory/scoped_ptr.h"
#include "cc/layers/video_frame_provider.h"
#include "content/renderer/gpu/stream_texture_host_android.h"

namespace WebKit {
class WebGraphicsContext3D;
}

namespace content {

// The proxy class for the gpu thread to notify the compositor thread
// when a new video frame is available.
class StreamTextureProxy : public StreamTextureHost::Listener {
 public:
  explicit StreamTextureProxy(StreamTextureHost* host);
  virtual ~StreamTextureProxy();

  // Initialize and bind to the current thread, which becomes the thread that
  // a connected client will receive callbacks on.
  void BindToCurrentThread(int stream_id, int width, int height);

  bool IsBoundToThread() { return loop_.get() != NULL; }

  // Setting the target for callback when a frame is available. This function
  // could be called on both the main thread and the compositor thread.
  void SetClient(cc::VideoFrameProvider::Client* client);

  // StreamTextureHost::Listener implementation:
  virtual void OnFrameAvailable() OVERRIDE;
  virtual void OnMatrixChanged(const float matrix[16]) OVERRIDE;

  struct Deleter {
    inline void operator()(StreamTextureProxy* ptr) const { ptr->Release(); }
  };

 private:
  // Causes this instance to be deleted on the thread it is bound to.
  void Release();

  scoped_ptr<StreamTextureHost> host_;
  scoped_refptr<base::MessageLoopProxy> loop_;

  base::Lock client_lock_;
  cc::VideoFrameProvider::Client* client_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamTextureProxy);
};

typedef scoped_ptr<StreamTextureProxy, StreamTextureProxy::Deleter>
    ScopedStreamTextureProxy;

// Factory class for managing stream textures.
class StreamTextureFactory {
 public:
  StreamTextureFactory(WebKit::WebGraphicsContext3D* context,
                       GpuChannelHost* channel,
                       int view_id);
  ~StreamTextureFactory();

  // Create the StreamTextureProxy object.
  StreamTextureProxy* CreateProxy();

  // Send an IPC message to the browser process to request a java surface
  // object for the given stream_id. After the the surface is created,
  // it will be passed back to the WebMediaPlayerAndroid object identified by
  // the player_id.
  void EstablishPeer(int stream_id, int player_id);

  // Create the streamTexture and return the stream Id and set the texture id.
  unsigned CreateStreamTexture(unsigned* texture_id);

  // Destroy the streamTexture for the given texture Id.
  void DestroyStreamTexture(unsigned texture_id);

 private:
  WebKit::WebGraphicsContext3D* context_;
  scoped_refptr<GpuChannelHost> channel_;
  int view_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamTextureFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_ANDROID_H_
