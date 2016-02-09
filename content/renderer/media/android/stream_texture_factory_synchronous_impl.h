// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_SYNCHRONOUS_IMPL_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_SYNCHRONOUS_IMPL_H_

#include <stdint.h>

#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "content/renderer/media/android/stream_texture_factory.h"

namespace gfx {
class SurfaceTexture;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}  //  namespace gles2
}  //  namespace gpu

namespace content {

// Factory for when using synchronous compositor in Android WebView.
class StreamTextureFactorySynchronousImpl : public StreamTextureFactory {
 public:
  class ContextProvider : public base::RefCountedThreadSafe<ContextProvider> {
   public:
    virtual scoped_refptr<gfx::SurfaceTexture> GetSurfaceTexture(
        uint32_t stream_id) = 0;

    virtual uint32_t CreateStreamTexture(uint32_t texture_id) = 0;

    virtual gpu::gles2::GLES2Interface* ContextGL() = 0;

    virtual void AddObserver(StreamTextureFactoryContextObserver* obs) = 0;
    virtual void RemoveObserver(StreamTextureFactoryContextObserver* obs) = 0;

   protected:
    friend class base::RefCountedThreadSafe<ContextProvider>;
    virtual ~ContextProvider() {}
  };

  typedef base::Callback<scoped_refptr<ContextProvider>(void)>
      CreateContextProviderCallback;

  static scoped_refptr<StreamTextureFactorySynchronousImpl> Create(
      const CreateContextProviderCallback& try_create_callback);

  StreamTextureProxy* CreateProxy() override;
  void EstablishPeer(int32_t stream_id, int player_id, int frame_id) override;
  unsigned CreateStreamTexture(unsigned texture_target,
                               unsigned* texture_id,
                               gpu::Mailbox* texture_mailbox) override;
  void SetStreamTextureSize(int32_t stream_id, const gfx::Size& size) override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  void AddObserver(StreamTextureFactoryContextObserver* obs) override;
  void RemoveObserver(StreamTextureFactoryContextObserver* obs) override;

 private:
  friend class base::RefCounted<StreamTextureFactorySynchronousImpl>;
  StreamTextureFactorySynchronousImpl(
      const CreateContextProviderCallback& try_create_callback);
  ~StreamTextureFactorySynchronousImpl() override;

  CreateContextProviderCallback create_context_provider_callback_;
  scoped_refptr<ContextProvider> context_provider_;
  std::set<StreamTextureFactoryContextObserver*> observers_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamTextureFactorySynchronousImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_STREAM_TEXTURE_FACTORY_SYNCHRONOUS_IMPL_H_
