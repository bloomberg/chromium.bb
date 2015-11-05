// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/cdm_adapter.h"

#include "base/logging.h"
#include "media/cdm/cdm_adapter_impl.h"

namespace media {

CdmAdapter::CdmAdapter() {}

CdmAdapter::~CdmAdapter() {}

void CdmAdapter::Initialize(
    const std::string& key_system,
    const base::FilePath& cdm_path,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const LegacySessionErrorCB& legacy_session_error_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    scoped_ptr<SimpleCdmPromise> promise) {
  DCHECK(!key_system.empty());
  DCHECK(!session_message_cb.is_null());
  DCHECK(!session_closed_cb.is_null());
  DCHECK(!legacy_session_error_cb.is_null());
  DCHECK(!session_keys_change_cb.is_null());
  DCHECK(!session_expiration_update_cb.is_null());

  scoped_refptr<CdmAdapterImpl> cdm = new CdmAdapterImpl();
  cdm->Initialize(key_system, cdm_path, cdm_config, session_message_cb,
                  session_closed_cb, legacy_session_error_cb,
                  session_keys_change_cb, session_expiration_update_cb,
                  promise.Pass());
  cdm_ = cdm.Pass();
}

void CdmAdapter::SetServerCertificate(const std::vector<uint8_t>& certificate,
                                      scoped_ptr<SimpleCdmPromise> promise) {
  DCHECK(cdm_);
  cdm_->SetServerCertificate(certificate, promise.Pass());
}

void CdmAdapter::CreateSessionAndGenerateRequest(
    SessionType session_type,
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    scoped_ptr<NewSessionCdmPromise> promise) {
  DCHECK(cdm_);
  return cdm_->CreateSessionAndGenerateRequest(session_type, init_data_type,
                                               init_data, promise.Pass());
}

void CdmAdapter::LoadSession(SessionType session_type,
                             const std::string& session_id,
                             scoped_ptr<NewSessionCdmPromise> promise) {
  DCHECK(cdm_);
  DCHECK(!session_id.empty());
  return cdm_->LoadSession(session_type, session_id, promise.Pass());
}

void CdmAdapter::UpdateSession(const std::string& session_id,
                               const std::vector<uint8_t>& response,
                               scoped_ptr<SimpleCdmPromise> promise) {
  DCHECK(cdm_);
  DCHECK(!session_id.empty());
  DCHECK(!response.empty());
  return cdm_->UpdateSession(session_id, response, promise.Pass());
}

void CdmAdapter::CloseSession(const std::string& session_id,
                              scoped_ptr<SimpleCdmPromise> promise) {
  DCHECK(cdm_);
  DCHECK(!session_id.empty());
  return cdm_->CloseSession(session_id, promise.Pass());
}

void CdmAdapter::RemoveSession(const std::string& session_id,
                               scoped_ptr<SimpleCdmPromise> promise) {
  DCHECK(cdm_);
  DCHECK(!session_id.empty());
  return cdm_->RemoveSession(session_id, promise.Pass());
}

CdmContext* CdmAdapter::GetCdmContext() {
  DCHECK(cdm_);
  return cdm_->GetCdmContext();
}

}  // namespace media
