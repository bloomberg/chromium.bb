// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/proxy_media_keys.h"

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "content/renderer/media/android/webmediaplayer_proxy_android.h"
#include "content/renderer/media/crypto/key_systems.h"

namespace content {

ProxyMediaKeys::ProxyMediaKeys(WebMediaPlayerProxyAndroid* proxy,
                               int media_keys_id)
    : proxy_(proxy), media_keys_id_(media_keys_id) {
  DCHECK(proxy_);
}

void ProxyMediaKeys::InitializeCDM(const std::string& key_system) {
  std::vector<uint8> uuid = GetUUID(key_system);
  DCHECK(!uuid.empty());
  proxy_->InitializeCDM(media_keys_id_, uuid);
}

bool ProxyMediaKeys::GenerateKeyRequest(const std::string& type,
                                        const uint8* init_data,
                                        int init_data_length) {
  proxy_->GenerateKeyRequest(
      media_keys_id_,
      type,
      std::vector<uint8>(init_data, init_data + init_data_length));
  return true;
}

void ProxyMediaKeys::AddKey(const uint8* key, int key_length,
                            const uint8* init_data, int init_data_length,
                            const std::string& session_id) {
  proxy_->AddKey(media_keys_id_,
                 std::vector<uint8>(key, key + key_length),
                 std::vector<uint8>(init_data, init_data + init_data_length),
                 session_id);
}

void ProxyMediaKeys::CancelKeyRequest(const std::string& session_id) {
  proxy_->CancelKeyRequest(media_keys_id_, session_id);
}

}  // namespace content
