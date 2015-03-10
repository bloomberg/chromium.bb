// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/cdm_session_adapter.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "media/base/cdm_factory.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/base/key_systems.h"
#include "media/base/media_keys.h"
#include "media/blink/webcontentdecryptionmodulesession_impl.h"
#include "url/gurl.h"

namespace media {

const char kMediaEME[] = "Media.EME.";
const char kDot[] = ".";

CdmSessionAdapter::CdmSessionAdapter() : weak_ptr_factory_(this) {
}

CdmSessionAdapter::~CdmSessionAdapter() {}

bool CdmSessionAdapter::Initialize(CdmFactory* cdm_factory,
                                   const std::string& key_system,
                                   bool allow_distinctive_identifier,
                                   bool allow_persistent_state,
                                   const GURL& security_origin) {
  key_system_ = key_system;
  key_system_uma_prefix_ =
      kMediaEME + GetKeySystemNameForUMA(key_system) + kDot;

  base::WeakPtr<CdmSessionAdapter> weak_this = weak_ptr_factory_.GetWeakPtr();
  media_keys_ = cdm_factory->Create(
      key_system, allow_distinctive_identifier, allow_persistent_state,
      security_origin,
      base::Bind(&CdmSessionAdapter::OnSessionMessage, weak_this),
      base::Bind(&CdmSessionAdapter::OnSessionClosed, weak_this),
      base::Bind(&CdmSessionAdapter::OnSessionError, weak_this),
      base::Bind(&CdmSessionAdapter::OnSessionKeysChange, weak_this),
      base::Bind(&CdmSessionAdapter::OnSessionExpirationUpdate, weak_this));
  return media_keys_.get() != nullptr;
}

void CdmSessionAdapter::SetServerCertificate(
    const uint8* server_certificate,
    int server_certificate_length,
    scoped_ptr<SimpleCdmPromise> promise) {
  media_keys_->SetServerCertificate(
      server_certificate, server_certificate_length, promise.Pass());
}

WebContentDecryptionModuleSessionImpl* CdmSessionAdapter::CreateSession() {
  return new WebContentDecryptionModuleSessionImpl(this);
}

bool CdmSessionAdapter::RegisterSession(
    const std::string& session_id,
    base::WeakPtr<WebContentDecryptionModuleSessionImpl> session) {
  // If this session ID is already registered, don't register it again.
  if (ContainsKey(sessions_, session_id))
    return false;

  sessions_[session_id] = session;
  return true;
}

void CdmSessionAdapter::UnregisterSession(const std::string& session_id) {
  DCHECK(ContainsKey(sessions_, session_id));
  sessions_.erase(session_id);
}

void CdmSessionAdapter::InitializeNewSession(
    const std::string& init_data_type,
    const uint8* init_data,
    int init_data_length,
    MediaKeys::SessionType session_type,
    scoped_ptr<NewSessionCdmPromise> promise) {
  media_keys_->CreateSessionAndGenerateRequest(session_type, init_data_type,
                                               init_data, init_data_length,
                                               promise.Pass());
}

void CdmSessionAdapter::LoadSession(MediaKeys::SessionType session_type,
                                    const std::string& session_id,
                                    scoped_ptr<NewSessionCdmPromise> promise) {
  media_keys_->LoadSession(session_type, session_id, promise.Pass());
}

void CdmSessionAdapter::UpdateSession(const std::string& session_id,
                                      const uint8* response,
                                      int response_length,
                                      scoped_ptr<SimpleCdmPromise> promise) {
  media_keys_->UpdateSession(session_id, response, response_length,
                             promise.Pass());
}

void CdmSessionAdapter::CloseSession(const std::string& session_id,
                                     scoped_ptr<SimpleCdmPromise> promise) {
  media_keys_->CloseSession(session_id, promise.Pass());
}

void CdmSessionAdapter::RemoveSession(const std::string& session_id,
                                      scoped_ptr<SimpleCdmPromise> promise) {
  media_keys_->RemoveSession(session_id, promise.Pass());
}

CdmContext* CdmSessionAdapter::GetCdmContext() {
  return media_keys_->GetCdmContext();
}

const std::string& CdmSessionAdapter::GetKeySystem() const {
  return key_system_;
}

const std::string& CdmSessionAdapter::GetKeySystemUMAPrefix() const {
  return key_system_uma_prefix_;
}

void CdmSessionAdapter::OnSessionMessage(
    const std::string& session_id,
    MediaKeys::MessageType message_type,
    const std::vector<uint8>& message,
    const GURL& /* legacy_destination_url */) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(session_id);
  DLOG_IF(WARNING, !session) << __FUNCTION__ << " for unknown session "
                             << session_id;
  if (session)
    session->OnSessionMessage(message_type, message);
}

void CdmSessionAdapter::OnSessionKeysChange(const std::string& session_id,
                                            bool has_additional_usable_key,
                                            CdmKeysInfo keys_info) {
  // TODO(jrummell): Pass |keys_info| on.
  WebContentDecryptionModuleSessionImpl* session = GetSession(session_id);
  DLOG_IF(WARNING, !session) << __FUNCTION__ << " for unknown session "
                             << session_id;
  if (session)
    session->OnSessionKeysChange(has_additional_usable_key, keys_info.Pass());
}

void CdmSessionAdapter::OnSessionExpirationUpdate(
    const std::string& session_id,
    const base::Time& new_expiry_time) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(session_id);
  DLOG_IF(WARNING, !session) << __FUNCTION__ << " for unknown session "
                             << session_id;
  if (session)
    session->OnSessionExpirationUpdate(new_expiry_time);
}

void CdmSessionAdapter::OnSessionClosed(const std::string& session_id) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(session_id);
  DLOG_IF(WARNING, !session) << __FUNCTION__ << " for unknown session "
                             << session_id;
  if (session)
    session->OnSessionClosed();
}

void CdmSessionAdapter::OnSessionError(const std::string& session_id,
                                       MediaKeys::Exception exception_code,
                                       uint32 system_code,
                                       const std::string& error_message) {
  // Error events not used by unprefixed EME.
  // TODO(jrummell): Remove when prefixed EME removed.
}

WebContentDecryptionModuleSessionImpl* CdmSessionAdapter::GetSession(
    const std::string& session_id) {
  // Since session objects may get garbage collected, it is possible that there
  // are events coming back from the CDM and the session has been unregistered.
  // We can not tell if the CDM is firing events at sessions that never existed.
  SessionMap::iterator session = sessions_.find(session_id);
  return (session != sessions_.end()) ? session->second.get() : NULL;
}

}  // namespace media
