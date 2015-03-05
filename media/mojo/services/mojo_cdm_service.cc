// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_service.h"

#include "base/bind.h"
#include "media/base/cdm_key_information.h"
#include "media/base/key_systems.h"
#include "media/cdm/aes_decryptor.h"
#include "media/mojo/services/media_type_converters.h"
#include "media/mojo/services/mojo_cdm_promise.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/url_type_converters.h"

namespace media {

typedef MojoCdmPromise<> SimpleMojoCdmPromise;
typedef MojoCdmPromise<std::string> NewSessionMojoCdmPromise;

MojoCdmService::MojoCdmService(const mojo::String& key_system)
    : weak_factory_(this) {
  base::WeakPtr<MojoCdmService> weak_this = weak_factory_.GetWeakPtr();

  // TODO(xhwang): Client syntax has been removed, so a new mechanism for client
  // discovery must be added to this interface.  See http://crbug.com/451321.
  NOTREACHED();

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

void MojoCdmService::CreateSessionAndGenerateRequest(
    mojo::ContentDecryptionModule::SessionType session_type,
    const mojo::String& init_data_type,
    mojo::Array<uint8_t> init_data,
    const mojo::Callback<void(mojo::CdmPromiseResultPtr, mojo::String)>&
        callback) {
  const std::vector<uint8_t>& init_data_vector = init_data.storage();
  cdm_->CreateSessionAndGenerateRequest(
      static_cast<MediaKeys::SessionType>(session_type),
      init_data_type.To<std::string>(),
      init_data_vector.empty() ? nullptr : &init_data_vector[0],
      init_data_vector.size(),
      scoped_ptr<NewSessionCdmPromise>(new NewSessionMojoCdmPromise(callback)));
}

void MojoCdmService::LoadSession(
    mojo::ContentDecryptionModule::SessionType session_type,
    const mojo::String& session_id,
    const mojo::Callback<void(mojo::CdmPromiseResultPtr, mojo::String)>&
        callback) {
  cdm_->LoadSession(
      static_cast<MediaKeys::SessionType>(session_type),
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

void MojoCdmService::GetCdmContext(
    int32_t cdm_id,
    mojo::InterfaceRequest<mojo::Decryptor> decryptor) {
  NOTIMPLEMENTED();
}

void MojoCdmService::OnSessionMessage(const std::string& session_id,
                                      MediaKeys::MessageType message_type,
                                      const std::vector<uint8_t>& message,
                                      const GURL& legacy_destination_url) {
  client_->OnSessionMessage(session_id,
                            static_cast<mojo::CdmMessageType>(message_type),
                            mojo::Array<uint8_t>::From(message),
                            mojo::String::From(legacy_destination_url));
}

void MojoCdmService::OnSessionKeysChange(const std::string& session_id,
                                         bool has_additional_usable_key,
                                         CdmKeysInfo keys_info) {
  mojo::Array<mojo::CdmKeyInformationPtr> keys_data;
  for (const auto& key : keys_info)
    keys_data.push_back(mojo::CdmKeyInformation::From(*key));
  client_->OnSessionKeysChange(session_id, has_additional_usable_key,
                               keys_data.Pass());
}

void MojoCdmService::OnSessionExpirationUpdate(
    const std::string& session_id,
    const base::Time& new_expiry_time) {
  client_->OnSessionExpirationUpdate(session_id,
                                     new_expiry_time.ToInternalValue());
}

void MojoCdmService::OnSessionClosed(const std::string& session_id) {
  client_->OnSessionClosed(session_id);
}

void MojoCdmService::OnSessionError(const std::string& session_id,
                                    MediaKeys::Exception exception,
                                    uint32_t system_code,
                                    const std::string& error_message) {
  client_->OnSessionError(session_id,
                          static_cast<mojo::CdmException>(exception),
                          system_code, error_message);
}

}  // namespace media
