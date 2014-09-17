// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/stream_texture_factory_synchronous_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "cc/output/context_provider.h"
#include "content/common/android/surface_texture_peer.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gl/android/surface_texture.h"

using gpu::gles2::GLES2Interface;

namespace content {

namespace {

class StreamTextureProxyImpl
    : public StreamTextureProxy,
      public base::SupportsWeakPtr<StreamTextureProxyImpl> {
 public:
  explicit StreamTextureProxyImpl(
      StreamTextureFactorySynchronousImpl::ContextProvider* provider);
  virtual ~StreamTextureProxyImpl();

  // StreamTextureProxy implementation:
  virtual void BindToLoop(int32 stream_id,
                          cc::VideoFrameProvider::Client* client,
                          scoped_refptr<base::MessageLoopProxy> loop) OVERRIDE;
  virtual void Release() OVERRIDE;

 private:
  void SetClient(cc::VideoFrameProvider::Client* client);
  void BindOnThread(int32 stream_id,
                    scoped_refptr<base::MessageLoopProxy> loop);
  void OnFrameAvailable();

  base::Lock client_lock_;
  cc::VideoFrameProvider::Client* client_;

  // Accessed on the |loop_| thread only.
  scoped_refptr<base::MessageLoopProxy> loop_;
  base::Closure callback_;
  scoped_refptr<StreamTextureFactorySynchronousImpl::ContextProvider>
      context_provider_;
  scoped_refptr<gfx::SurfaceTexture> surface_texture_;
  float current_matrix_[16];
  bool has_updated_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamTextureProxyImpl);
};

StreamTextureProxyImpl::StreamTextureProxyImpl(
    StreamTextureFactorySynchronousImpl::ContextProvider* provider)
    : client_(NULL), context_provider_(provider), has_updated_(false) {
  std::fill(current_matrix_, current_matrix_ + 16, 0);
}

StreamTextureProxyImpl::~StreamTextureProxyImpl() {}

void StreamTextureProxyImpl::Release() {
  // Assumes this is the last reference to this object. So no need to acquire
  // lock.
  SetClient(NULL);
  if (!loop_.get() || loop_->BelongsToCurrentThread() ||
      !loop_->DeleteSoon(FROM_HERE, this)) {
    delete this;
  }
}

void StreamTextureProxyImpl::SetClient(cc::VideoFrameProvider::Client* client) {
  base::AutoLock lock(client_lock_);
  client_ = client;
}

void StreamTextureProxyImpl::BindToLoop(
    int32 stream_id,
    cc::VideoFrameProvider::Client* client,
    scoped_refptr<base::MessageLoopProxy> loop) {
  DCHECK(loop);
  SetClient(client);
  if (loop->BelongsToCurrentThread()) {
    BindOnThread(stream_id, loop);
    return;
  }
  // Unretained is safe here only because the object is deleted on |loop_|
  // thread.
  loop->PostTask(FROM_HERE,
                 base::Bind(&StreamTextureProxyImpl::BindOnThread,
                            base::Unretained(this),
                            stream_id,
                            loop));
}

void StreamTextureProxyImpl::BindOnThread(
    int32 stream_id,
    scoped_refptr<base::MessageLoopProxy> loop) {
  DCHECK(!loop_ || (loop == loop_));
  loop_ = loop;

  surface_texture_ = context_provider_->GetSurfaceTexture(stream_id);
  if (!surface_texture_) {
    LOG(ERROR) << "Failed to get SurfaceTexture for stream.";
    return;
  }

  callback_ =
      base::Bind(&StreamTextureProxyImpl::OnFrameAvailable, AsWeakPtr());
  surface_texture_->SetFrameAvailableCallback(callback_);
}

void StreamTextureProxyImpl::OnFrameAvailable() {
  // GetTransformMatrix only returns something valid after both is true:
  // - OnFrameAvailable was called
  // - we called UpdateTexImage
  if (has_updated_) {
    float matrix[16];
    surface_texture_->GetTransformMatrix(matrix);

    if (memcmp(current_matrix_, matrix, sizeof(matrix)) != 0) {
      memcpy(current_matrix_, matrix, sizeof(matrix));

      base::AutoLock lock(client_lock_);
      if (client_)
        client_->DidUpdateMatrix(current_matrix_);
    }
  }
  // OnFrameAvailable being called a second time implies that we called
  // updateTexImage since after we received the first frame.
  has_updated_ = true;

  base::AutoLock lock(client_lock_);
  if (client_)
    client_->DidReceiveFrame();
}

}  // namespace

// static
scoped_refptr<StreamTextureFactorySynchronousImpl>
StreamTextureFactorySynchronousImpl::Create(
    const CreateContextProviderCallback& try_create_callback,
    int frame_id) {
  return new StreamTextureFactorySynchronousImpl(try_create_callback, frame_id);
}

StreamTextureFactorySynchronousImpl::StreamTextureFactorySynchronousImpl(
    const CreateContextProviderCallback& try_create_callback,
    int frame_id)
    : create_context_provider_callback_(try_create_callback),
      context_provider_(create_context_provider_callback_.Run()),
      frame_id_(frame_id),
      observer_(NULL) {}

StreamTextureFactorySynchronousImpl::~StreamTextureFactorySynchronousImpl() {}

StreamTextureProxy* StreamTextureFactorySynchronousImpl::CreateProxy() {
  if (!context_provider_)
    context_provider_ = create_context_provider_callback_.Run();

  if (!context_provider_)
    return NULL;

  if (observer_)
    context_provider_->AddObserver(observer_);
  return new StreamTextureProxyImpl(context_provider_);
}

void StreamTextureFactorySynchronousImpl::EstablishPeer(int32 stream_id,
                                                        int player_id) {
  DCHECK(context_provider_);
  scoped_refptr<gfx::SurfaceTexture> surface_texture =
      context_provider_->GetSurfaceTexture(stream_id);
  if (surface_texture) {
    SurfaceTexturePeer::GetInstance()->EstablishSurfaceTexturePeer(
        base::Process::Current().handle(),
        surface_texture,
        frame_id_,
        player_id);
  }
}

unsigned StreamTextureFactorySynchronousImpl::CreateStreamTexture(
    unsigned texture_target,
    unsigned* texture_id,
    gpu::Mailbox* texture_mailbox) {
  DCHECK(context_provider_);
  unsigned stream_id = 0;
  GLES2Interface* gl = context_provider_->ContextGL();
  gl->GenTextures(1, texture_id);
  stream_id = gl->CreateStreamTextureCHROMIUM(*texture_id);

  gl->GenMailboxCHROMIUM(texture_mailbox->name);
  gl->ProduceTextureDirectCHROMIUM(
      *texture_id, texture_target, texture_mailbox->name);
  return stream_id;
}

void StreamTextureFactorySynchronousImpl::SetStreamTextureSize(
    int32 stream_id,
    const gfx::Size& size) {}

gpu::gles2::GLES2Interface* StreamTextureFactorySynchronousImpl::ContextGL() {
  DCHECK(context_provider_);
  return context_provider_->ContextGL();
}

void StreamTextureFactorySynchronousImpl::AddObserver(
    StreamTextureFactoryContextObserver* obs) {
  DCHECK(!observer_);
  observer_ = obs;
  if (context_provider_)
    context_provider_->AddObserver(obs);
}

void StreamTextureFactorySynchronousImpl::RemoveObserver(
    StreamTextureFactoryContextObserver* obs) {
  DCHECK_EQ(observer_, obs);
  observer_ = NULL;
  if (context_provider_)
    context_provider_->AddObserver(obs);
}

}  // namespace content
