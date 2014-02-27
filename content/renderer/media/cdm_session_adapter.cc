// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/cdm_session_adapter.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "content/renderer/media/crypto/content_decryption_module_factory.h"
#include "content/renderer/media/webcontentdecryptionmodulesession_impl.h"
#include "media/base/media_keys.h"
#include "url/gurl.h"

namespace content {

const uint32 kStartingSessionId = 1;
uint32 CdmSessionAdapter::next_session_id_ = kStartingSessionId;
COMPILE_ASSERT(kStartingSessionId > media::MediaKeys::kInvalidSessionId,
               invalid_starting_value);

CdmSessionAdapter::CdmSessionAdapter() : weak_ptr_factory_(this) {}

CdmSessionAdapter::~CdmSessionAdapter() {}

bool CdmSessionAdapter::Initialize(
#if defined(ENABLE_PEPPER_CDMS)
    const CreatePepperCdmCB& create_pepper_cdm_cb,
#endif
    const std::string& key_system) {
  base::WeakPtr<CdmSessionAdapter> weak_this = weak_ptr_factory_.GetWeakPtr();
  media_keys_ =
      ContentDecryptionModuleFactory::Create(
          key_system,
#if defined(ENABLE_PEPPER_CDMS)
          create_pepper_cdm_cb,
#elif defined(OS_ANDROID)
          // TODO(xhwang): Support Android.
          NULL,
          0,
          // TODO(ddorwin): Get the URL for the frame containing the MediaKeys.
          GURL(),
#endif  // defined(ENABLE_PEPPER_CDMS)
          base::Bind(&CdmSessionAdapter::OnSessionCreated, weak_this),
          base::Bind(&CdmSessionAdapter::OnSessionMessage, weak_this),
          base::Bind(&CdmSessionAdapter::OnSessionReady, weak_this),
          base::Bind(&CdmSessionAdapter::OnSessionClosed, weak_this),
          base::Bind(&CdmSessionAdapter::OnSessionError, weak_this));

  // Success if |media_keys_| created.
  return media_keys_;
}

WebContentDecryptionModuleSessionImpl* CdmSessionAdapter::CreateSession(
    blink::WebContentDecryptionModuleSession::Client* client) {
  // Generate a unique internal session id for the new session.
  uint32 session_id = next_session_id_++;
  DCHECK(sessions_.find(session_id) == sessions_.end());
  WebContentDecryptionModuleSessionImpl* session =
      new WebContentDecryptionModuleSessionImpl(session_id, client, this);
  sessions_[session_id] = session;
  return session;
}

void CdmSessionAdapter::RemoveSession(uint32 session_id) {
  DCHECK(sessions_.find(session_id) != sessions_.end());
  sessions_.erase(session_id);
}

void CdmSessionAdapter::InitializeNewSession(uint32 session_id,
                                             const std::string& content_type,
                                             const uint8* init_data,
                                             int init_data_length) {
  DCHECK(sessions_.find(session_id) != sessions_.end());
  media_keys_->CreateSession(
      session_id, content_type, init_data, init_data_length);
}

void CdmSessionAdapter::UpdateSession(uint32 session_id,
                                      const uint8* response,
                                      int response_length) {
  DCHECK(sessions_.find(session_id) != sessions_.end());
  media_keys_->UpdateSession(session_id, response, response_length);
}

void CdmSessionAdapter::ReleaseSession(uint32 session_id) {
  DCHECK(sessions_.find(session_id) != sessions_.end());
  media_keys_->ReleaseSession(session_id);
}

media::Decryptor* CdmSessionAdapter::GetDecryptor() {
  return media_keys_->GetDecryptor();
}

void CdmSessionAdapter::OnSessionCreated(uint32 session_id,
                                         const std::string& web_session_id) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(session_id);
  DLOG_IF(WARNING, !session) << __FUNCTION__ << " for unknown session "
                             << session_id;
  if (session)
    session->OnSessionCreated(web_session_id);
}

void CdmSessionAdapter::OnSessionMessage(uint32 session_id,
                                         const std::vector<uint8>& message,
                                         const std::string& destination_url) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(session_id);
  DLOG_IF(WARNING, !session) << __FUNCTION__ << " for unknown session "
                             << session_id;
  if (session)
    session->OnSessionMessage(message, destination_url);
}

void CdmSessionAdapter::OnSessionReady(uint32 session_id) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(session_id);
  DLOG_IF(WARNING, !session) << __FUNCTION__ << " for unknown session "
                             << session_id;
  if (session)
    session->OnSessionReady();
}

void CdmSessionAdapter::OnSessionClosed(uint32 session_id) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(session_id);
  DLOG_IF(WARNING, !session) << __FUNCTION__ << " for unknown session "
                             << session_id;
  if (session)
    session->OnSessionClosed();
}

void CdmSessionAdapter::OnSessionError(uint32 session_id,
                                       media::MediaKeys::KeyError error_code,
                                       int system_code) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(session_id);
  DLOG_IF(WARNING, !session) << __FUNCTION__ << " for unknown session "
                             << session_id;
  if (session)
    session->OnSessionError(error_code, system_code);
}

WebContentDecryptionModuleSessionImpl* CdmSessionAdapter::GetSession(
    uint32 session_id) {
  // Since session objects may get garbage collected, it is possible that there
  // are events coming back from the CDM and the session has been unregistered.
  // We can not tell if the CDM is firing events at sessions that never existed.
  SessionMap::iterator session = sessions_.find(session_id);
  return (session != sessions_.end()) ? session->second : NULL;
}

}  // namespace content
