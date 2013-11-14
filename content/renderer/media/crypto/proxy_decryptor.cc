// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/proxy_decryptor.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/renderer/media/crypto/content_decryption_module_factory.h"
#if defined(OS_ANDROID)
#include "content/renderer/media/android/renderer_media_player_manager.h"
#endif  // defined(OS_ANDROID)

namespace content {

// Since these reference IDs may conflict with the ones generated in
// WebContentDecryptionModuleSessionImpl for the short time both paths are
// active, start with 100000 and generate the IDs from there.
// TODO(jrummell): Only allow one path http://crbug.com/306680.
uint32 ProxyDecryptor::next_reference_id_ = 100000;

const uint32 INVALID_REFERENCE_ID = 0;

#if defined(ENABLE_PEPPER_CDMS)
void ProxyDecryptor::DestroyHelperPlugin() {
  ContentDecryptionModuleFactory::DestroyHelperPlugin(
      web_media_player_client_, web_frame_);
}
#endif  // defined(ENABLE_PEPPER_CDMS)

ProxyDecryptor::ProxyDecryptor(
#if defined(ENABLE_PEPPER_CDMS)
    blink::WebMediaPlayerClient* web_media_player_client,
    blink::WebFrame* web_frame,
#elif defined(OS_ANDROID)
    RendererMediaPlayerManager* manager,
    int media_keys_id,
#endif  // defined(ENABLE_PEPPER_CDMS)
    const KeyAddedCB& key_added_cb,
    const KeyErrorCB& key_error_cb,
    const KeyMessageCB& key_message_cb)
    : weak_ptr_factory_(this),
#if defined(ENABLE_PEPPER_CDMS)
      web_media_player_client_(web_media_player_client),
      web_frame_(web_frame),
#elif defined(OS_ANDROID)
      manager_(manager),
      media_keys_id_(media_keys_id),
#endif  // defined(ENABLE_PEPPER_CDMS)
      key_added_cb_(key_added_cb),
      key_error_cb_(key_error_cb),
      key_message_cb_(key_message_cb) {
  DCHECK(!key_added_cb_.is_null());
  DCHECK(!key_error_cb_.is_null());
  DCHECK(!key_message_cb_.is_null());
}

ProxyDecryptor::~ProxyDecryptor() {
  // Destroy the decryptor explicitly before destroying the plugin.
  {
    base::AutoLock auto_lock(lock_);
    media_keys_.reset();
  }
}

// TODO(xhwang): Support multiple decryptor notification request (e.g. from
// video and audio decoders). The current implementation is okay for the current
// media pipeline since we initialize audio and video decoders in sequence.
// But ProxyDecryptor should not depend on media pipeline's implementation
// detail.
void ProxyDecryptor::SetDecryptorReadyCB(
     const media::DecryptorReadyCB& decryptor_ready_cb) {
  base::AutoLock auto_lock(lock_);

  // Cancels the previous decryptor request.
  if (decryptor_ready_cb.is_null()) {
    if (!decryptor_ready_cb_.is_null())
      base::ResetAndReturn(&decryptor_ready_cb_).Run(NULL);
    return;
  }

  // Normal decryptor request.
  DCHECK(decryptor_ready_cb_.is_null());
  if (media_keys_) {
    decryptor_ready_cb.Run(media_keys_->GetDecryptor());
    return;
  }
  decryptor_ready_cb_ = decryptor_ready_cb;
}

bool ProxyDecryptor::InitializeCDM(const std::string& key_system,
                                   const GURL& frame_url) {
  DVLOG(1) << "InitializeCDM: key_system = " << key_system;

  base::AutoLock auto_lock(lock_);

  DCHECK(!media_keys_);
  media_keys_ = CreateMediaKeys(key_system, frame_url);
  if (!media_keys_)
    return false;

  if (!decryptor_ready_cb_.is_null())
    base::ResetAndReturn(&decryptor_ready_cb_).Run(media_keys_->GetDecryptor());
  return true;
}

bool ProxyDecryptor::GenerateKeyRequest(const std::string& type,
                                        const uint8* init_data,
                                        int init_data_length) {
  // Use a unique reference id for this request.
  uint32 reference_id = next_reference_id_++;
  if (!media_keys_->GenerateKeyRequest(
           reference_id, type, init_data, init_data_length)) {
    media_keys_.reset();
    return false;
  }

  return true;
}

void ProxyDecryptor::AddKey(const uint8* key,
                            int key_length,
                            const uint8* init_data,
                            int init_data_length,
                            const std::string& session_id) {
  DVLOG(1) << "AddKey()";

  // WebMediaPlayerImpl ensures GenerateKeyRequest() has been called.
  uint32 reference_id = LookupReferenceId(session_id);
  if (reference_id == INVALID_REFERENCE_ID) {
    // Session hasn't been referenced before, so it is an error.
    // Note that the specification says "If sessionId is not null and is
    // unrecognized, throw an INVALID_ACCESS_ERR." However, for backwards
    // compatibility the error is not thrown, but rather reported as a
    // KeyError.
    key_error_cb_.Run(
        std::string(), media::MediaKeys::kUnknownError, 0);
  }
  else {
    media_keys_->AddKey(
        reference_id, key, key_length, init_data, init_data_length);
  }
}

void ProxyDecryptor::CancelKeyRequest(const std::string& session_id) {
  DVLOG(1) << "CancelKeyRequest()";

  // WebMediaPlayerImpl ensures GenerateKeyRequest() has been called.
  uint32 reference_id = LookupReferenceId(session_id);
  if (reference_id == INVALID_REFERENCE_ID) {
    // Session hasn't been created, so it is an error.
    key_error_cb_.Run(
        std::string(), media::MediaKeys::kUnknownError, 0);
  }
  else {
    media_keys_->CancelKeyRequest(reference_id);
  }
}

scoped_ptr<media::MediaKeys> ProxyDecryptor::CreateMediaKeys(
    const std::string& key_system,
    const GURL& frame_url) {
  return ContentDecryptionModuleFactory::Create(
      key_system,
#if defined(ENABLE_PEPPER_CDMS)
      web_media_player_client_,
      web_frame_,
      base::Bind(&ProxyDecryptor::DestroyHelperPlugin,
                 weak_ptr_factory_.GetWeakPtr()),
#elif defined(OS_ANDROID)
      manager_,
      media_keys_id_,
      frame_url,
#endif  // defined(ENABLE_PEPPER_CDMS)
      base::Bind(&ProxyDecryptor::KeyAdded, weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ProxyDecryptor::KeyError, weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ProxyDecryptor::KeyMessage, weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ProxyDecryptor::SetSessionId,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ProxyDecryptor::KeyAdded(uint32 reference_id) {
  // Assumes that SetSessionId() has been called before this.
  key_added_cb_.Run(LookupSessionId(reference_id));
}

void ProxyDecryptor::KeyError(uint32 reference_id,
                              media::MediaKeys::KeyError error_code,
                              int system_code) {
  // Assumes that SetSessionId() has been called before this.
  key_error_cb_.Run(LookupSessionId(reference_id), error_code, system_code);
}

void ProxyDecryptor::KeyMessage(uint32 reference_id,
                                const std::vector<uint8>& message,
                                const std::string& default_url) {
  // Assumes that SetSessionId() has been called before this.
  key_message_cb_.Run(LookupSessionId(reference_id), message, default_url);
}

void ProxyDecryptor::SetSessionId(uint32 reference_id,
                                  const std::string& session_id) {
  // Due to heartbeat messages, SetSessionId() can get called multiple times.
  SessionIdMap::iterator it = sessions_.find(reference_id);
  DCHECK(it == sessions_.end() || it->second == session_id);
  if (it == sessions_.end())
    sessions_[reference_id] = session_id;
}

uint32 ProxyDecryptor::LookupReferenceId(const std::string& session_id) {
  for (SessionIdMap::iterator it = sessions_.begin();
       it != sessions_.end();
       ++it) {
    if (it->second == session_id)
      return it->first;
  }

  // If |session_id| is null, then use the single reference id.
  if (session_id.empty() && sessions_.size() == 1)
    return sessions_.begin()->first;

  return INVALID_REFERENCE_ID;
}

const std::string& ProxyDecryptor::LookupSessionId(uint32 reference_id) {
  DCHECK_NE(reference_id, INVALID_REFERENCE_ID);

  // Session may not exist if error happens during GenerateKeyRequest().
  SessionIdMap::iterator it = sessions_.find(reference_id);
  return (it != sessions_.end()) ? it->second : EmptyString();
}

}  // namespace content
