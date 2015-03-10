// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/default_cdm_factory.h"

#include "media/base/key_systems.h"
#include "media/cdm/aes_decryptor.h"

namespace media {

DefaultCdmFactory::DefaultCdmFactory() {
}

DefaultCdmFactory::~DefaultCdmFactory() {
}

scoped_ptr<MediaKeys> DefaultCdmFactory::Create(
    const std::string& key_system,
    bool allow_distinctive_identifier,
    bool allow_persistent_state,
    const GURL& security_origin,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionErrorCB& session_error_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb) {
  if (CanUseAesDecryptor(key_system)) {
    return make_scoped_ptr(new AesDecryptor(
        session_message_cb, session_closed_cb, session_keys_change_cb));
  }

  return nullptr;
}

}  // namespace media
