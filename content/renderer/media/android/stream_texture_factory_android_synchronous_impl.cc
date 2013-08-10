// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/stream_texture_factory_android_synchronous_impl.h"

namespace content {

StreamTextureFactorySynchronousImpl::StreamTextureFactorySynchronousImpl() {}
StreamTextureFactorySynchronousImpl::~StreamTextureFactorySynchronousImpl() {}

StreamTextureProxy* StreamTextureFactorySynchronousImpl::CreateProxy() {
  return NULL;
}

void StreamTextureFactorySynchronousImpl::EstablishPeer(int32 stream_id,
                                                        int player_id) {}

unsigned StreamTextureFactorySynchronousImpl::CreateStreamTexture(
    unsigned texture_target,
    unsigned* texture_id,
    gpu::Mailbox* texture_mailbox,
    unsigned* texture_mailbox_sync_point) {
  return 0;
}

void StreamTextureFactorySynchronousImpl::DestroyStreamTexture(
    unsigned texture_id) {}

void StreamTextureFactorySynchronousImpl::SetStreamTextureSize(
    int32 texture_id,
    const gfx::Size& size) {}

}  // namespace content
