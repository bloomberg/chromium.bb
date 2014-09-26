// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/cdm_session_adapter.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "content/renderer/media/crypto/content_decryption_module_factory.h"
#include "content/renderer/media/crypto/key_systems.h"
#include "content/renderer/media/webcontentdecryptionmodulesession_impl.h"
#include "media/base/cdm_promise.h"
#include "media/base/media_keys.h"
#include "url/gurl.h"

namespace content {

const char kMediaEME[] = "Media.EME.";
const char kDot[] = ".";

CdmSessionAdapter::CdmSessionAdapter() :
#if defined(ENABLE_BROWSER_CDMS)
    cdm_id_(0),
#endif
    weak_ptr_factory_(this) {}

CdmSessionAdapter::~CdmSessionAdapter() {}

bool CdmSessionAdapter::Initialize(
#if defined(ENABLE_PEPPER_CDMS)
    const CreatePepperCdmCB& create_pepper_cdm_cb,
#elif defined(ENABLE_BROWSER_CDMS)
    RendererCdmManager* manager,
#endif  // defined(ENABLE_PEPPER_CDMS)
    const std::string& key_system,
    const GURL& security_origin) {
  key_system_uma_prefix_ = kMediaEME + KeySystemNameForUMA(key_system) + kDot;
  base::WeakPtr<CdmSessionAdapter> weak_this = weak_ptr_factory_.GetWeakPtr();
  media_keys_ = ContentDecryptionModuleFactory::Create(
      key_system,
      security_origin,
#if defined(ENABLE_PEPPER_CDMS)
      create_pepper_cdm_cb,
#elif defined(ENABLE_BROWSER_CDMS)
      manager,
      &cdm_id_,
#endif  // defined(ENABLE_PEPPER_CDMS)
      base::Bind(&CdmSessionAdapter::OnSessionMessage, weak_this),
      base::Bind(&CdmSessionAdapter::OnSessionReady, weak_this),
      base::Bind(&CdmSessionAdapter::OnSessionClosed, weak_this),
      base::Bind(&CdmSessionAdapter::OnSessionError, weak_this),
      base::Bind(&CdmSessionAdapter::OnSessionKeysChange, weak_this),
      base::Bind(&CdmSessionAdapter::OnSessionExpirationUpdate, weak_this));

  // Success if |media_keys_| created.
  return media_keys_;
}

void CdmSessionAdapter::SetServerCertificate(
    const uint8* server_certificate,
    int server_certificate_length,
    scoped_ptr<media::SimpleCdmPromise> promise) {
  media_keys_->SetServerCertificate(
      server_certificate, server_certificate_length, promise.Pass());
}

WebContentDecryptionModuleSessionImpl* CdmSessionAdapter::CreateSession() {
  return new WebContentDecryptionModuleSessionImpl(this);
}

bool CdmSessionAdapter::RegisterSession(
    const std::string& web_session_id,
    base::WeakPtr<WebContentDecryptionModuleSessionImpl> session) {
  // If this session ID is already registered, don't register it again.
  if (ContainsKey(sessions_, web_session_id))
    return false;

  sessions_[web_session_id] = session;
  return true;
}

void CdmSessionAdapter::UnregisterSession(const std::string& web_session_id) {
  DCHECK(ContainsKey(sessions_, web_session_id));
  sessions_.erase(web_session_id);
}

void CdmSessionAdapter::InitializeNewSession(
    const std::string& init_data_type,
    const uint8* init_data,
    int init_data_length,
    media::MediaKeys::SessionType session_type,
    scoped_ptr<media::NewSessionCdmPromise> promise) {
  media_keys_->CreateSession(init_data_type,
                             init_data,
                             init_data_length,
                             session_type,
                             promise.Pass());
}

void CdmSessionAdapter::UpdateSession(
    const std::string& web_session_id,
    const uint8* response,
    int response_length,
    scoped_ptr<media::SimpleCdmPromise> promise) {
  media_keys_->UpdateSession(
      web_session_id, response, response_length, promise.Pass());
}

void CdmSessionAdapter::CloseSession(
    const std::string& web_session_id,
    scoped_ptr<media::SimpleCdmPromise> promise) {
  media_keys_->CloseSession(web_session_id, promise.Pass());
}

void CdmSessionAdapter::RemoveSession(
    const std::string& web_session_id,
    scoped_ptr<media::SimpleCdmPromise> promise) {
  media_keys_->RemoveSession(web_session_id, promise.Pass());
}

void CdmSessionAdapter::GetUsableKeyIds(
    const std::string& web_session_id,
    scoped_ptr<media::KeyIdsPromise> promise) {
  media_keys_->GetUsableKeyIds(web_session_id, promise.Pass());
}

media::Decryptor* CdmSessionAdapter::GetDecryptor() {
  return media_keys_->GetDecryptor();
}

const std::string& CdmSessionAdapter::GetKeySystemUMAPrefix() const {
  return key_system_uma_prefix_;
}

#if defined(ENABLE_BROWSER_CDMS)
int CdmSessionAdapter::GetCdmId() const {
  return cdm_id_;
}
#endif  // defined(ENABLE_BROWSER_CDMS)

void CdmSessionAdapter::OnSessionMessage(const std::string& web_session_id,
                                         const std::vector<uint8>& message,
                                         const GURL& destination_url) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(web_session_id);
  DLOG_IF(WARNING, !session) << __FUNCTION__ << " for unknown session "
                             << web_session_id;
  if (session)
    session->OnSessionMessage(message, destination_url);
}

void CdmSessionAdapter::OnSessionKeysChange(const std::string& web_session_id,
                                            bool has_additional_usable_key) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(web_session_id);
  DLOG_IF(WARNING, !session) << __FUNCTION__ << " for unknown session "
                             << web_session_id;
  if (session)
    session->OnSessionKeysChange(has_additional_usable_key);
}

void CdmSessionAdapter::OnSessionExpirationUpdate(
    const std::string& web_session_id,
    const base::Time& new_expiry_time) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(web_session_id);
  DLOG_IF(WARNING, !session) << __FUNCTION__ << " for unknown session "
                             << web_session_id;
  if (session)
    session->OnSessionExpirationUpdate(new_expiry_time);
}

void CdmSessionAdapter::OnSessionReady(const std::string& web_session_id) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(web_session_id);
  DLOG_IF(WARNING, !session) << __FUNCTION__ << " for unknown session "
                             << web_session_id;
  if (session)
    session->OnSessionReady();
}

void CdmSessionAdapter::OnSessionClosed(const std::string& web_session_id) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(web_session_id);
  DLOG_IF(WARNING, !session) << __FUNCTION__ << " for unknown session "
                             << web_session_id;
  if (session)
    session->OnSessionClosed();
}

void CdmSessionAdapter::OnSessionError(
    const std::string& web_session_id,
    media::MediaKeys::Exception exception_code,
    uint32 system_code,
    const std::string& error_message) {
  WebContentDecryptionModuleSessionImpl* session = GetSession(web_session_id);
  DLOG_IF(WARNING, !session) << __FUNCTION__ << " for unknown session "
                             << web_session_id;
  if (session)
    session->OnSessionError(exception_code, system_code, error_message);
}

WebContentDecryptionModuleSessionImpl* CdmSessionAdapter::GetSession(
    const std::string& web_session_id) {
  // Since session objects may get garbage collected, it is possible that there
  // are events coming back from the CDM and the session has been unregistered.
  // We can not tell if the CDM is firing events at sessions that never existed.
  SessionMap::iterator session = sessions_.find(web_session_id);
  return (session != sessions_.end()) ? session->second.get() : NULL;
}

}  // namespace content
