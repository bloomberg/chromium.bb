// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/stream_texture_factory_synchronous_impl.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
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
  ~StreamTextureProxyImpl() override;

  // StreamTextureProxy implementation:
  void BindToLoop(int32_t stream_id,
                  cc::VideoFrameProvider::Client* client,
                  scoped_refptr<base::SingleThreadTaskRunner> loop) override;
  void Release() override;

 private:
  void BindOnThread(int32_t stream_id);
  void OnFrameAvailable();

  // Protects access to |client_| and |loop_|.
  base::Lock lock_;
  cc::VideoFrameProvider::Client* client_;
  scoped_refptr<base::SingleThreadTaskRunner> loop_;

  // Accessed on the |loop_| thread only.
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
  {
    // Cannot call into |client_| anymore (from any thread) after returning
    // from here.
    base::AutoLock lock(lock_);
    client_ = NULL;
  }
  // Release is analogous to the destructor, so there should be no more external
  // calls to this object in Release. Therefore there is no need to acquire the
  // lock to access |loop_|.
  if (!loop_.get() || loop_->BelongsToCurrentThread() ||
      !loop_->DeleteSoon(FROM_HERE, this)) {
    delete this;
  }
}

void StreamTextureProxyImpl::BindToLoop(
    int32_t stream_id,
    cc::VideoFrameProvider::Client* client,
    scoped_refptr<base::SingleThreadTaskRunner> loop) {
  DCHECK(loop.get());

  {
    base::AutoLock lock(lock_);
    DCHECK(!loop_.get() || (loop.get() == loop_.get()));
    loop_ = loop;
    client_ = client;
  }

  if (loop->BelongsToCurrentThread()) {
    BindOnThread(stream_id);
    return;
  }
  // Unretained is safe here only because the object is deleted on |loop_|
  // thread.
  loop->PostTask(FROM_HERE,
                 base::Bind(&StreamTextureProxyImpl::BindOnThread,
                            base::Unretained(this),
                            stream_id));
}

void StreamTextureProxyImpl::BindOnThread(int32_t stream_id) {
  surface_texture_ = context_provider_->GetSurfaceTexture(stream_id);
  if (!surface_texture_.get()) {
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

      base::AutoLock lock(lock_);
      if (client_)
        client_->DidUpdateMatrix(current_matrix_);
    }
  }
  // OnFrameAvailable being called a second time implies that we called
  // updateTexImage since after we received the first frame.
  has_updated_ = true;

  base::AutoLock lock(lock_);
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
  bool had_proxy = !!context_provider_.get();
  if (!had_proxy)
    context_provider_ = create_context_provider_callback_.Run();

  if (!context_provider_.get())
    return NULL;

  if (observer_ && !had_proxy)
    context_provider_->AddObserver(observer_);
  return new StreamTextureProxyImpl(context_provider_.get());
}

void StreamTextureFactorySynchronousImpl::EstablishPeer(int32_t stream_id,
                                                        int player_id,
                                                        int frame_id) {
  DCHECK(context_provider_.get());
  scoped_refptr<gfx::SurfaceTexture> surface_texture =
      context_provider_->GetSurfaceTexture(stream_id);
  if (surface_texture.get()) {
    SurfaceTexturePeer::GetInstance()->EstablishSurfaceTexturePeer(
        base::GetCurrentProcessHandle(),
        surface_texture,
        frame_id_,
        player_id);
  }
}

unsigned StreamTextureFactorySynchronousImpl::CreateStreamTexture(
    unsigned texture_target,
    unsigned* texture_id,
    gpu::Mailbox* texture_mailbox) {
  DCHECK(context_provider_.get());
  unsigned stream_id = 0;
  GLES2Interface* gl = context_provider_->ContextGL();
  gl->GenTextures(1, texture_id);
  gl->ShallowFlushCHROMIUM();
  stream_id = context_provider_->CreateStreamTexture(*texture_id);
  gl->GenMailboxCHROMIUM(texture_mailbox->name);
  gl->ProduceTextureDirectCHROMIUM(
      *texture_id, texture_target, texture_mailbox->name);
  return stream_id;
}

void StreamTextureFactorySynchronousImpl::SetStreamTextureSize(
    int32_t stream_id,
    const gfx::Size& size) {}

gpu::gles2::GLES2Interface* StreamTextureFactorySynchronousImpl::ContextGL() {
  DCHECK(context_provider_.get());
  return context_provider_->ContextGL();
}

void StreamTextureFactorySynchronousImpl::AddObserver(
    StreamTextureFactoryContextObserver* obs) {
  DCHECK(!observer_);
  observer_ = obs;
  if (context_provider_.get())
    context_provider_->AddObserver(obs);
}

void StreamTextureFactorySynchronousImpl::RemoveObserver(
    StreamTextureFactoryContextObserver* obs) {
  DCHECK_EQ(observer_, obs);
  observer_ = NULL;
  if (context_provider_.get())
    context_provider_->RemoveObserver(obs);
}

}  // namespace content
