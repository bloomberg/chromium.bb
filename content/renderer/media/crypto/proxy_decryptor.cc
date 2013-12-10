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
#include "media/cdm/json_web_key.h"
#include "media/cdm/key_system_names.h"

namespace content {

// Since these reference IDs may conflict with the ones generated in
// WebContentDecryptionModuleSessionImpl for the short time both paths are
// active, start with 100000 and generate the IDs from there.
// TODO(jrummell): Only allow one path http://crbug.com/306680.
uint32 ProxyDecryptor::next_session_id_ = 100000;

const uint32 kInvalidSessionId = 0;

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
      key_message_cb_(key_message_cb),
      is_clear_key_(false) {
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

  is_clear_key_ =
      media::IsClearKey(key_system) || media::IsExternalClearKey(key_system);
  return true;
}

bool ProxyDecryptor::GenerateKeyRequest(const std::string& type,
                                        const uint8* init_data,
                                        int init_data_length) {
  // Use a unique reference id for this request.
  uint32 session_id = next_session_id_++;
  if (!media_keys_->CreateSession(
           session_id, type, init_data, init_data_length)) {
    media_keys_.reset();
    return false;
  }

  return true;
}

void ProxyDecryptor::AddKey(const uint8* key,
                            int key_length,
                            const uint8* init_data,
                            int init_data_length,
                            const std::string& web_session_id) {
  DVLOG(1) << "AddKey()";

  // WebMediaPlayerImpl ensures GenerateKeyRequest() has been called.
  uint32 session_id = LookupSessionId(web_session_id);
  if (session_id == kInvalidSessionId) {
    // Session hasn't been referenced before, so it is an error.
    // Note that the specification says "If sessionId is not null and is
    // unrecognized, throw an INVALID_ACCESS_ERR." However, for backwards
    // compatibility the error is not thrown, but rather reported as a
    // KeyError.
    key_error_cb_.Run(std::string(), media::MediaKeys::kUnknownError, 0);
    return;
  }

  // EME WD spec only supports a single array passed to the CDM. For
  // Clear Key using v0.1b, both arrays are used (|init_data| is key_id).
  // Since the EME WD spec supports the key as a JSON Web Key,
  // convert the 2 arrays to a JWK and pass it as the single array.
  if (is_clear_key_) {
    // Decryptor doesn't support empty key ID (see http://crbug.com/123265).
    // So ensure a non-empty value is passed.
    if (!init_data) {
      static const uint8 kDummyInitData[1] = {0};
      init_data = kDummyInitData;
      init_data_length = arraysize(kDummyInitData);
    }

    std::string jwk =
        media::GenerateJWKSet(key, key_length, init_data, init_data_length);
    DCHECK(!jwk.empty());
    media_keys_->UpdateSession(
        session_id, reinterpret_cast<const uint8*>(jwk.data()), jwk.size());
    return;
  }

  media_keys_->UpdateSession(session_id, key, key_length);
}

void ProxyDecryptor::CancelKeyRequest(const std::string& session_id) {
  DVLOG(1) << "CancelKeyRequest()";

  // WebMediaPlayerImpl ensures GenerateKeyRequest() has been called.
  uint32 session_reference_id = LookupSessionId(session_id);
  if (session_reference_id == kInvalidSessionId) {
    // Session hasn't been created, so it is an error.
    key_error_cb_.Run(
        std::string(), media::MediaKeys::kUnknownError, 0);
  }
  else {
    media_keys_->ReleaseSession(session_reference_id);
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
      base::Bind(&ProxyDecryptor::OnSessionCreated,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ProxyDecryptor::OnSessionMessage,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ProxyDecryptor::OnSessionReady,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ProxyDecryptor::OnSessionClosed,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ProxyDecryptor::OnSessionError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ProxyDecryptor::OnSessionCreated(uint32 session_id,
                                      const std::string& web_session_id) {
  // Due to heartbeat messages, OnSessionCreated() can get called multiple
  // times.
  SessionIdMap::iterator it = sessions_.find(session_id);
  DCHECK(it == sessions_.end() || it->second == web_session_id);
  if (it == sessions_.end())
    sessions_[session_id] = web_session_id;
}

void ProxyDecryptor::OnSessionMessage(uint32 session_id,
                                      const std::vector<uint8>& message,
                                      const std::string& destination_url) {
  // Assumes that OnSessionCreated() has been called before this.
  key_message_cb_.Run(LookupWebSessionId(session_id), message, destination_url);
}

void ProxyDecryptor::OnSessionReady(uint32 session_id) {
  // Assumes that OnSessionCreated() has been called before this.
  key_added_cb_.Run(LookupWebSessionId(session_id));
}

void ProxyDecryptor::OnSessionClosed(uint32 session_id) {
  // No closed event in EME v0.1b.
}

void ProxyDecryptor::OnSessionError(uint32 session_id,
                                    media::MediaKeys::KeyError error_code,
                                    int system_code) {
  // Assumes that OnSessionCreated() has been called before this.
  key_error_cb_.Run(LookupWebSessionId(session_id), error_code, system_code);
}

uint32 ProxyDecryptor::LookupSessionId(const std::string& session_id) {
  for (SessionIdMap::iterator it = sessions_.begin();
       it != sessions_.end();
       ++it) {
    if (it->second == session_id)
      return it->first;
  }

  // If |session_id| is null, then use the single reference id.
  if (session_id.empty() && sessions_.size() == 1)
    return sessions_.begin()->first;

  return kInvalidSessionId;
}

const std::string& ProxyDecryptor::LookupWebSessionId(uint32 session_id) {
  DCHECK_NE(session_id, kInvalidSessionId);

  // Session may not exist if error happens during GenerateKeyRequest().
  SessionIdMap::iterator it = sessions_.find(session_id);
  return (it != sessions_.end()) ? it->second : base::EmptyString();
}

}  // namespace content
