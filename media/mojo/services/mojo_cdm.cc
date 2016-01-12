// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/mojo/interfaces/decryptor.mojom.h"
#include "media/mojo/services/media_type_converters.h"
#include "media/mojo/services/mojo_decryptor.h"
#include "mojo/shell/public/cpp/connect.h"
#include "mojo/shell/public/interfaces/service_provider.mojom.h"
#include "url/gurl.h"

namespace media {

template <typename PromiseType>
static void RejectPromise(scoped_ptr<PromiseType> promise,
                          interfaces::CdmPromiseResultPtr result) {
  promise->reject(static_cast<MediaKeys::Exception>(result->exception),
                  result->system_code, result->error_message);
}

// static
void MojoCdm::Create(
    const std::string& key_system,
    const GURL& security_origin,
    const media::CdmConfig& cdm_config,
    interfaces::ContentDecryptionModulePtr remote_cdm,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::LegacySessionErrorCB& legacy_session_error_cb,
    const media::SessionKeysChangeCB& session_keys_change_cb,
    const media::SessionExpirationUpdateCB& session_expiration_update_cb,
    const media::CdmCreatedCB& cdm_created_cb) {
  scoped_refptr<MojoCdm> mojo_cdm(
      new MojoCdm(std::move(remote_cdm), session_message_cb, session_closed_cb,
                  legacy_session_error_cb, session_keys_change_cb,
                  session_expiration_update_cb));

  // |mojo_cdm| ownership is passed to the promise.
  scoped_ptr<CdmInitializedPromise> promise(
      new CdmInitializedPromise(cdm_created_cb, mojo_cdm));

  mojo_cdm->InitializeCdm(key_system, security_origin, cdm_config,
                          std::move(promise));
}

MojoCdm::MojoCdm(interfaces::ContentDecryptionModulePtr remote_cdm,
                 const SessionMessageCB& session_message_cb,
                 const SessionClosedCB& session_closed_cb,
                 const LegacySessionErrorCB& legacy_session_error_cb,
                 const SessionKeysChangeCB& session_keys_change_cb,
                 const SessionExpirationUpdateCB& session_expiration_update_cb)
    : remote_cdm_(std::move(remote_cdm)),
      binding_(this),
      cdm_id_(CdmContext::kInvalidCdmId),
      session_message_cb_(session_message_cb),
      session_closed_cb_(session_closed_cb),
      legacy_session_error_cb_(legacy_session_error_cb),
      session_keys_change_cb_(session_keys_change_cb),
      session_expiration_update_cb_(session_expiration_update_cb),
      weak_factory_(this) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(!session_message_cb_.is_null());
  DCHECK(!session_closed_cb_.is_null());
  DCHECK(!legacy_session_error_cb_.is_null());
  DCHECK(!session_keys_change_cb_.is_null());
  DCHECK(!session_expiration_update_cb_.is_null());

  interfaces::ContentDecryptionModuleClientPtr client_ptr;
  binding_.Bind(GetProxy(&client_ptr));
  remote_cdm_->SetClient(std::move(client_ptr));
}

MojoCdm::~MojoCdm() {
  DVLOG(1) << __FUNCTION__;
}

void MojoCdm::InitializeCdm(const std::string& key_system,
                            const GURL& security_origin,
                            const media::CdmConfig& cdm_config,
                            scoped_ptr<CdmInitializedPromise> promise) {
  DVLOG(1) << __FUNCTION__ << ": " << key_system;
  remote_cdm_->Initialize(
      key_system, security_origin.spec(),
      interfaces::CdmConfig::From(cdm_config),
      base::Bind(&MojoCdm::OnCdmInitialized, weak_factory_.GetWeakPtr(),
                 base::Passed(&promise)));
}

void MojoCdm::SetServerCertificate(const std::vector<uint8_t>& certificate,
                                   scoped_ptr<SimpleCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  remote_cdm_->SetServerCertificate(
      mojo::Array<uint8_t>::From(certificate),
      base::Bind(&MojoCdm::OnPromiseResult<>, weak_factory_.GetWeakPtr(),
                 base::Passed(&promise)));
}

void MojoCdm::CreateSessionAndGenerateRequest(
    SessionType session_type,
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    scoped_ptr<NewSessionCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  remote_cdm_->CreateSessionAndGenerateRequest(
      static_cast<interfaces::ContentDecryptionModule::SessionType>(
          session_type),
      static_cast<interfaces::ContentDecryptionModule::InitDataType>(
          init_data_type),
      mojo::Array<uint8_t>::From(init_data),
      base::Bind(&MojoCdm::OnPromiseResult<std::string>,
                 weak_factory_.GetWeakPtr(), base::Passed(&promise)));
}

void MojoCdm::LoadSession(SessionType session_type,
                          const std::string& session_id,
                          scoped_ptr<NewSessionCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  remote_cdm_->LoadSession(
      static_cast<interfaces::ContentDecryptionModule::SessionType>(
          session_type),
      session_id,
      base::Bind(&MojoCdm::OnPromiseResult<std::string>,
                 weak_factory_.GetWeakPtr(), base::Passed(&promise)));
}

void MojoCdm::UpdateSession(const std::string& session_id,
                            const std::vector<uint8_t>& response,
                            scoped_ptr<SimpleCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  remote_cdm_->UpdateSession(
      session_id, mojo::Array<uint8_t>::From(response),
      base::Bind(&MojoCdm::OnPromiseResult<>, weak_factory_.GetWeakPtr(),
                 base::Passed(&promise)));
}

void MojoCdm::CloseSession(const std::string& session_id,
                           scoped_ptr<SimpleCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  remote_cdm_->CloseSession(session_id, base::Bind(&MojoCdm::OnPromiseResult<>,
                                                   weak_factory_.GetWeakPtr(),
                                                   base::Passed(&promise)));
}

void MojoCdm::RemoveSession(const std::string& session_id,
                            scoped_ptr<SimpleCdmPromise> promise) {
  DVLOG(2) << __FUNCTION__;
  remote_cdm_->RemoveSession(session_id, base::Bind(&MojoCdm::OnPromiseResult<>,
                                                    weak_factory_.GetWeakPtr(),
                                                    base::Passed(&promise)));
}

CdmContext* MojoCdm::GetCdmContext() {
  DVLOG(2) << __FUNCTION__;
  return this;
}

media::Decryptor* MojoCdm::GetDecryptor() {
  if (decryptor_ptr_) {
    DCHECK(!decryptor_);
    decryptor_.reset(new MojoDecryptor(std::move(decryptor_ptr_)));
  }

  return decryptor_.get();
}

int MojoCdm::GetCdmId() const {
  DCHECK_NE(CdmContext::kInvalidCdmId, cdm_id_);
  return cdm_id_;
}

void MojoCdm::OnSessionMessage(const mojo::String& session_id,
                               interfaces::CdmMessageType message_type,
                               mojo::Array<uint8_t> message,
                               const mojo::String& legacy_destination_url) {
  DVLOG(2) << __FUNCTION__;
  GURL verified_gurl = GURL(legacy_destination_url);
  if (!verified_gurl.is_valid() && !verified_gurl.is_empty()) {
    DLOG(WARNING) << "SessionMessage destination_url is invalid : "
                  << verified_gurl.possibly_invalid_spec();
    verified_gurl = GURL::EmptyGURL();  // Replace invalid destination_url.
  }

  session_message_cb_.Run(session_id,
                          static_cast<MediaKeys::MessageType>(message_type),
                          message.storage(), verified_gurl);
}

void MojoCdm::OnSessionClosed(const mojo::String& session_id) {
  DVLOG(2) << __FUNCTION__;
  session_closed_cb_.Run(session_id);
}

void MojoCdm::OnLegacySessionError(const mojo::String& session_id,
                                   interfaces::CdmException exception,
                                   uint32_t system_code,
                                   const mojo::String& error_message) {
  DVLOG(2) << __FUNCTION__;
  legacy_session_error_cb_.Run(session_id,
                               static_cast<MediaKeys::Exception>(exception),
                               system_code, error_message);
}

void MojoCdm::OnSessionKeysChange(
    const mojo::String& session_id,
    bool has_additional_usable_key,
    mojo::Array<interfaces::CdmKeyInformationPtr> keys_info) {
  DVLOG(2) << __FUNCTION__;

  // TODO(jrummell): Handling resume playback should be done in the media
  // player, not in the Decryptors. http://crbug.com/413413.
  if (has_additional_usable_key && decryptor_)
    decryptor_->OnKeyAdded();

  media::CdmKeysInfo key_data;
  key_data.reserve(keys_info.size());
  for (size_t i = 0; i < keys_info.size(); ++i) {
    key_data.push_back(
        keys_info[i].To<scoped_ptr<media::CdmKeyInformation>>().release());
  }
  session_keys_change_cb_.Run(session_id, has_additional_usable_key,
                              std::move(key_data));
}

void MojoCdm::OnSessionExpirationUpdate(const mojo::String& session_id,
                                        double new_expiry_time_sec) {
  DVLOG(2) << __FUNCTION__;
  session_expiration_update_cb_.Run(
      session_id, base::Time::FromDoubleT(new_expiry_time_sec));
}

void MojoCdm::OnCdmInitialized(scoped_ptr<CdmInitializedPromise> promise,
                               interfaces::CdmPromiseResultPtr result,
                               int cdm_id,
                               interfaces::DecryptorPtr decryptor) {
  DVLOG(2) << __FUNCTION__ << " cdm_id: " << cdm_id;
  if (!result->success) {
    RejectPromise(std::move(promise), std::move(result));
    return;
  }

  DCHECK_NE(CdmContext::kInvalidCdmId, cdm_id);
  cdm_id_ = cdm_id;
  decryptor_ptr_ = std::move(decryptor);
  promise->resolve();
}

}  // namespace media
