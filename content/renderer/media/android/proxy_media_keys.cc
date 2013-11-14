// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/proxy_media_keys.h"

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "content/renderer/media/android/renderer_media_player_manager.h"
#include "content/renderer/media/crypto/key_systems.h"

namespace content {

ProxyMediaKeys::ProxyMediaKeys(RendererMediaPlayerManager* manager,
                               int media_keys_id,
                               const media::KeyAddedCB& key_added_cb,
                               const media::KeyErrorCB& key_error_cb,
                               const media::KeyMessageCB& key_message_cb,
                               const media::SetSessionIdCB& set_session_id_cb)
    : manager_(manager),
      media_keys_id_(media_keys_id),
      key_added_cb_(key_added_cb),
      key_error_cb_(key_error_cb),
      key_message_cb_(key_message_cb),
      set_session_id_cb_(set_session_id_cb) {
  DCHECK(manager_);
}

ProxyMediaKeys::~ProxyMediaKeys() {
}

void ProxyMediaKeys::InitializeCDM(const std::string& key_system,
                                   const GURL& frame_url) {
#if defined(ENABLE_PEPPER_CDMS)
  NOTIMPLEMENTED();
#elif defined(OS_ANDROID)
  std::vector<uint8> uuid = GetUUID(key_system);
  DCHECK(!uuid.empty());
  manager_->InitializeCDM(media_keys_id_, this, uuid, frame_url);
#endif
}

bool ProxyMediaKeys::GenerateKeyRequest(uint32 reference_id,
                                        const std::string& type,
                                        const uint8* init_data,
                                        int init_data_length) {
  manager_->GenerateKeyRequest(
      media_keys_id_,
      reference_id,
      type,
      std::vector<uint8>(init_data, init_data + init_data_length));
  return true;
}

void ProxyMediaKeys::AddKey(uint32 reference_id,
                            const uint8* key, int key_length,
                            const uint8* init_data, int init_data_length) {
  manager_->AddKey(media_keys_id_,
                   reference_id,
                   std::vector<uint8>(key, key + key_length),
                   std::vector<uint8>(init_data, init_data + init_data_length));
}

void ProxyMediaKeys::CancelKeyRequest(uint32 reference_id) {
  manager_->CancelKeyRequest(media_keys_id_, reference_id);
}

void ProxyMediaKeys::OnKeyAdded(uint32 reference_id) {
  key_added_cb_.Run(reference_id);
}

void ProxyMediaKeys::OnKeyError(uint32 reference_id,
                                media::MediaKeys::KeyError error_code,
                                int system_code) {
  key_error_cb_.Run(reference_id, error_code, system_code);
}

void ProxyMediaKeys::OnKeyMessage(uint32 reference_id,
                                  const std::vector<uint8>& message,
                                  const std::string& destination_url) {
  key_message_cb_.Run(reference_id, message, destination_url);
}

void ProxyMediaKeys::OnSetSessionId(uint32 reference_id,
                                    const std::string& session_id) {
  set_session_id_cb_.Run(reference_id, session_id);
}

}  // namespace content
