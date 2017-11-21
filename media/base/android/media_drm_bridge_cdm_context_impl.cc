// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_drm_bridge_cdm_context_impl.h"

#include "media/base/android/media_drm_bridge.h"

namespace media {

MediaDrmBridgeCdmContextImpl::MediaDrmBridgeCdmContextImpl(
    MediaDrmBridge* media_drm_bridge)
    : media_drm_bridge_(media_drm_bridge) {
  DCHECK(media_drm_bridge_);
}

MediaDrmBridgeCdmContextImpl::~MediaDrmBridgeCdmContextImpl() {}

Decryptor* MediaDrmBridgeCdmContextImpl::GetDecryptor() {
  return nullptr;
}

int MediaDrmBridgeCdmContextImpl::GetCdmId() const {
  return kInvalidCdmId;
}

int MediaDrmBridgeCdmContextImpl::RegisterPlayer(
    const base::Closure& new_key_cb,
    const base::Closure& cdm_unset_cb) {
  return media_drm_bridge_->RegisterPlayer(new_key_cb, cdm_unset_cb);
}

void MediaDrmBridgeCdmContextImpl::UnregisterPlayer(int registration_id) {
  media_drm_bridge_->UnregisterPlayer(registration_id);
}

void MediaDrmBridgeCdmContextImpl::SetMediaCryptoReadyCB(
    const MediaCryptoReadyCB& media_crypto_ready_cb) {
  media_drm_bridge_->SetMediaCryptoReadyCB(media_crypto_ready_cb);
}

}  // namespace media
