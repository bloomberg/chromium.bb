// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "cc/layers/video_frame_provider.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/gfx/size.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}  // namespace gles2
}  // namespace gpu

namespace content {

// The proxy class for the gpu thread to notify the compositor thread
// when a new video frame is available.
class StreamTextureProxy {
 public:
  virtual ~StreamTextureProxy() {}

  // Initialize and bind to the loop, which becomes the thread that
  // a connected client will receive callbacks on. This can be called
  // on any thread, but must be called with the same loop every time.
  virtual void BindToLoop(int32 stream_id,
                          cc::VideoFrameProvider::Client* client,
                          scoped_refptr<base::MessageLoopProxy> loop) = 0;

  // Causes this instance to be deleted on the thread it is bound to.
  virtual void Release() = 0;

  struct Deleter {
    inline void operator()(StreamTextureProxy* ptr) const { ptr->Release(); }
  };
};

typedef scoped_ptr<StreamTextureProxy, StreamTextureProxy::Deleter>
    ScopedStreamTextureProxy;

class StreamTextureFactoryContextObserver {
 public:
  virtual ~StreamTextureFactoryContextObserver() {}
  virtual void ResetStreamTextureProxy() = 0;
};

// Factory class for managing stream textures.
class StreamTextureFactory : public base::RefCounted<StreamTextureFactory> {
 public:
  // Create the StreamTextureProxy object.
  virtual StreamTextureProxy* CreateProxy() = 0;

  // Send an IPC message to the browser process to request a java surface
  // object for the given stream_id. After the the surface is created,
  // it will be passed back to the WebMediaPlayerAndroid object identified by
  // the player_id.
  virtual void EstablishPeer(int32 stream_id, int player_id) = 0;

  // Creates a StreamTexture and returns its id.  Sets |*texture_id| to the
  // client-side id of the StreamTexture. The texture is produced into
  // a mailbox so it can be shipped in a VideoFrame.
  virtual unsigned CreateStreamTexture(unsigned texture_target,
                                       unsigned* texture_id,
                                       gpu::Mailbox* texture_mailbox) = 0;

  // Set the streamTexture size for the given stream Id.
  virtual void SetStreamTextureSize(int32 texture_id,
                                    const gfx::Size& size) = 0;

  virtual gpu::gles2::GLES2Interface* ContextGL() = 0;

  virtual void AddObserver(StreamTextureFactoryContextObserver* obs) = 0;
  virtual void RemoveObserver(StreamTextureFactoryContextObserver* obs) = 0;

 protected:
  friend class base::RefCounted<StreamTextureFactory>;
  virtual ~StreamTextureFactory() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_H_
