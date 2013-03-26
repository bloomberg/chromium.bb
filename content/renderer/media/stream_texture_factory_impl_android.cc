// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream_texture_factory_impl_android.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "content/common/android/surface_texture_peer.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "ui/gfx/size.h"

namespace {

static void DeleteStreamTextureHost(content::StreamTextureHost* host) {
  delete host;
}

// Implementation of the StreamTextureProxy class. This class listens to all
// the stream texture updates and forward them to the
// cc::VideoFrameProvider::Client.
class StreamTextureProxyImpl : public webkit_media::StreamTextureProxy,
                               public content::StreamTextureHost::Listener {
 public:
  explicit StreamTextureProxyImpl(content::StreamTextureHost* host);
  virtual ~StreamTextureProxyImpl();

  // webkit_media::StreamTextureProxy implementation:
  virtual bool Initialize(int stream_id, int width, int height) OVERRIDE;
  virtual bool IsInitialized() OVERRIDE { return initialized_; }
  virtual void SetClient(cc::VideoFrameProvider::Client* client) OVERRIDE;

  // StreamTextureHost::Listener implementation:
  virtual void OnFrameAvailable() OVERRIDE;
  virtual void OnMatrixChanged(const float matrix[16]) OVERRIDE;

 private:
  scoped_ptr<content::StreamTextureHost> host_;
  scoped_refptr<base::MessageLoopProxy> loop_;

  base::Lock client_lock_;
  cc::VideoFrameProvider::Client* client_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(StreamTextureProxyImpl);
};

StreamTextureProxyImpl::StreamTextureProxyImpl(
    content::StreamTextureHost* host)
    : host_(host),
      client_(NULL),
      initialized_(false) {
  DCHECK(host);
  host->SetListener(this);
}

StreamTextureProxyImpl::~StreamTextureProxyImpl() {
  SetClient(NULL);
  // The StreamTextureHost instance needs to be deleted on the thread
  // it receives messages on (where it uses a WeakPtr).
  if (loop_.get()) {
    loop_->PostTask(FROM_HERE, base::Bind(&DeleteStreamTextureHost,
                                          host_.release()));
  }
}

void StreamTextureProxyImpl::SetClient(cc::VideoFrameProvider::Client* client) {
  base::AutoLock lock(client_lock_);
  client_ = client;
}

bool StreamTextureProxyImpl::Initialize(int stream_id, int width, int height) {
  loop_ = base::MessageLoopProxy::current();
  initialized_ = true;
  return host_->Initialize(stream_id, gfx::Size(width, height));
}

void StreamTextureProxyImpl::OnFrameAvailable() {
  base::AutoLock lock(client_lock_);
  if (client_)
    client_->DidReceiveFrame();
}

void StreamTextureProxyImpl::OnMatrixChanged(const float matrix[16]) {
  base::AutoLock lock(client_lock_);
  if (client_)
    client_->DidUpdateMatrix(matrix);
}

}  // anonymous namespace

namespace content {

StreamTextureFactoryImpl::StreamTextureFactoryImpl(
    WebKit::WebGraphicsContext3D* context,
    GpuChannelHost* channel,
    int view_id)
    : context_(context),
      channel_(channel),
      view_id_(view_id) {
  DCHECK(context_);
  DCHECK(channel);
}

StreamTextureFactoryImpl::~StreamTextureFactoryImpl() {
}

webkit_media::StreamTextureProxy* StreamTextureFactoryImpl::CreateProxy() {
  DCHECK(channel_.get());
  StreamTextureHost* host = new StreamTextureHost(channel_.get());
  return new StreamTextureProxyImpl(host);
}

void StreamTextureFactoryImpl::EstablishPeer(int stream_id, int player_id) {
  DCHECK(channel_.get());
  channel_->Send(new GpuChannelMsg_EstablishStreamTexture(
      stream_id, view_id_, player_id));
}

unsigned StreamTextureFactoryImpl::CreateStreamTexture(unsigned* texture_id) {
  unsigned stream_id = 0;
  if (context_->makeContextCurrent()) {
    *texture_id = context_->createTexture();
    stream_id = context_->createStreamTextureCHROMIUM(*texture_id);
    context_->flush();
  }
  return stream_id;
}

void StreamTextureFactoryImpl::DestroyStreamTexture(unsigned texture_id) {
  if (context_->makeContextCurrent()) {
    context_->destroyStreamTextureCHROMIUM(texture_id);
    context_->deleteTexture(texture_id);
    context_->flush();
  }
}

}  // namespace content
