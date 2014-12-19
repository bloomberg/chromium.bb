// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_service.h"

#include "base/bind.h"
#include "media/base/key_systems.h"
#include "media/cdm/aes_decryptor.h"
#include "media/mojo/services/mojo_cdm_promise.h"
#include "mojo/common/common_type_converters.h"

namespace media {

typedef MojoCdmPromise<> SimpleMojoCdmPromise;
typedef MojoCdmPromise<std::string> NewSessionMojoCdmPromise;
typedef MojoCdmPromise<std::vector<std::vector<uint8_t>>> KeyIdsMojoCdmPromise;

MojoCdmService::MojoCdmService(const mojo::String& key_system)
    : weak_factory_(this) {
  base::WeakPtr<MojoCdmService> weak_this = weak_factory_.GetWeakPtr();
  if (CanUseAesDecryptor(key_system)) {
    cdm_.reset(new AesDecryptor(
        base::Bind(&MojoCdmService::OnSessionMessage, weak_this),
        base::Bind(&MojoCdmService::OnSessionClosed, weak_this),
        base::Bind(&MojoCdmService::OnSessionKeysChange, weak_this)));
  }

  // TODO(xhwang): Check key system support in the app.
  NOTREACHED();
}

MojoCdmService::~MojoCdmService() {
}

// mojo::MediaRenderer implementation.
void MojoCdmService::SetServerCertificate(
    mojo::Array<uint8_t> certificate_data,
    const mojo::Callback<void(mojo::CdmPromiseResultPtr)>& callback) {
  const std::vector<uint8_t>& certificate_data_vector =
      certificate_data.storage();
  cdm_->SetServerCertificate(
      certificate_data_vector.empty() ? nullptr : &certificate_data_vector[0],
      certificate_data_vector.size(),
      scoped_ptr<SimpleCdmPromise>(new SimpleMojoCdmPromise(callback)));
}

void MojoCdmService::CreateSession(
    const mojo::String& init_data_type,
    mojo::Array<uint8_t> init_data,
    mojo::ContentDecryptionModule::SessionType session_type,
    const mojo::Callback<void(mojo::CdmPromiseResultPtr, mojo::String)>&
        callback) {
  const std::vector<uint8_t>& init_data_vector = init_data.storage();
  cdm_->CreateSession(
      init_data_type.To<std::string>(),
      init_data_vector.empty() ? nullptr : &init_data_vector[0],
      init_data_vector.size(),
      static_cast<MediaKeys::SessionType>(session_type),
      scoped_ptr<NewSessionCdmPromise>(new NewSessionMojoCdmPromise(callback)));
}

void MojoCdmService::LoadSession(
    const mojo::String& session_id,
    const mojo::Callback<void(mojo::CdmPromiseResultPtr, mojo::String)>&
        callback) {
  cdm_->LoadSession(
      session_id.To<std::string>(),
      scoped_ptr<NewSessionCdmPromise>(new NewSessionMojoCdmPromise(callback)));
}

void MojoCdmService::UpdateSession(
    const mojo::String& session_id,
    mojo::Array<uint8_t> response,
    const mojo::Callback<void(mojo::CdmPromiseResultPtr)>& callback) {
  const std::vector<uint8_t>& response_vector = response.storage();
  cdm_->UpdateSession(
      session_id.To<std::string>(),
      response_vector.empty() ? nullptr : &response_vector[0],
      response_vector.size(),
      scoped_ptr<SimpleCdmPromise>(new SimpleMojoCdmPromise(callback)));
}

void MojoCdmService::CloseSession(
    const mojo::String& session_id,
    const mojo::Callback<void(mojo::CdmPromiseResultPtr)>& callback) {
  cdm_->CloseSession(
      session_id.To<std::string>(),
      scoped_ptr<SimpleCdmPromise>(new SimpleMojoCdmPromise(callback)));
}

void MojoCdmService::RemoveSession(
    const mojo::String& session_id,
    const mojo::Callback<void(mojo::CdmPromiseResultPtr)>& callback) {
  cdm_->RemoveSession(
      session_id.To<std::string>(),
      scoped_ptr<SimpleCdmPromise>(new SimpleMojoCdmPromise(callback)));
}

void MojoCdmService::GetUsableKeyIds(
    const mojo::String& session_id,
    const mojo::Callback<void(mojo::CdmPromiseResultPtr,
                              mojo::Array<mojo::Array<uint8_t>>)>& callback) {
  cdm_->GetUsableKeyIds(
      session_id.To<std::string>(),
      scoped_ptr<KeyIdsPromise>(new KeyIdsMojoCdmPromise(callback)));
}

void MojoCdmService::GetCdmContext(
    int32_t cdm_id,
    mojo::InterfaceRequest<mojo::Decryptor> decryptor) {
  NOTIMPLEMENTED();
}

void MojoCdmService::OnSessionMessage(const std::string& session_id,
                                      const std::vector<uint8_t>& message,
                                      const GURL& destination_url) {
  client()->OnSessionMessage(session_id, mojo::Array<uint8_t>::From(message),
                             mojo::String::From(destination_url));
}

void MojoCdmService::OnSessionKeysChange(const std::string& session_id,
                                         bool has_additional_usable_key) {
  client()->OnSessionKeysChange(session_id, has_additional_usable_key);
}

void MojoCdmService::OnSessionExpirationUpdate(
    const std::string& session_id,
    const base::Time& new_expiry_time) {
  client()->OnSessionExpirationUpdate(session_id,
                                      new_expiry_time.ToInternalValue());
}

void MojoCdmService::OnSessionReady(const std::string& session_id) {
  client()->OnSessionReady(session_id);
}

void MojoCdmService::OnSessionClosed(const std::string& session_id) {
  client()->OnSessionClosed(session_id);
}

void MojoCdmService::OnSessionError(const std::string& session_id,
                                    MediaKeys::Exception exception,
                                    uint32_t system_code,
                                    const std::string& error_message) {
  client()->OnSessionError(session_id,
                           static_cast<mojo::CdmException>(exception),
                           system_code, error_message);
}

}  // namespace media
