// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webcontentdecryptionmodulesession_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/base/key_systems.h"
#include "media/base/media_keys.h"
#include "media/blink/cdm_result_promise.h"
#include "media/blink/cdm_session_adapter.h"
#include "media/blink/new_session_cdm_result_promise.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebEncryptedMediaKeyInformation.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace media {

const char kCloseSessionUMAName[] = "CloseSession";
const char kGenerateRequestUMAName[] = "GenerateRequest";
const char kLoadSessionUMAName[] = "LoadSession";
const char kRemoveSessionUMAName[] = "RemoveSession";
const char kUpdateSessionUMAName[] = "UpdateSession";

static blink::WebContentDecryptionModuleSession::Client::MessageType
convertMessageType(MediaKeys::MessageType message_type) {
  switch (message_type) {
    case media::MediaKeys::LICENSE_REQUEST:
      return blink::WebContentDecryptionModuleSession::Client::MessageType::
          LicenseRequest;
    case media::MediaKeys::LICENSE_RENEWAL:
      return blink::WebContentDecryptionModuleSession::Client::MessageType::
          LicenseRenewal;
    case media::MediaKeys::LICENSE_RELEASE:
      return blink::WebContentDecryptionModuleSession::Client::MessageType::
          LicenseRelease;
  }

  NOTREACHED();
  return blink::WebContentDecryptionModuleSession::Client::MessageType::
      LicenseRequest;
}

static blink::WebEncryptedMediaKeyInformation::KeyStatus convertStatus(
    media::CdmKeyInformation::KeyStatus status) {
  switch (status) {
    case media::CdmKeyInformation::USABLE:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::Usable;
    case media::CdmKeyInformation::INTERNAL_ERROR:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::InternalError;
    case media::CdmKeyInformation::EXPIRED:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::Expired;
    case media::CdmKeyInformation::OUTPUT_NOT_ALLOWED:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::
          OutputNotAllowed;
    case media::CdmKeyInformation::OUTPUT_DOWNSCALED:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::
          OutputDownscaled;
    case media::CdmKeyInformation::KEY_STATUS_PENDING:
      return blink::WebEncryptedMediaKeyInformation::KeyStatus::StatusPending;
  }

  NOTREACHED();
  return blink::WebEncryptedMediaKeyInformation::KeyStatus::InternalError;
}

WebContentDecryptionModuleSessionImpl::WebContentDecryptionModuleSessionImpl(
    const scoped_refptr<CdmSessionAdapter>& adapter)
    : adapter_(adapter), is_closed_(false), weak_ptr_factory_(this) {
}

WebContentDecryptionModuleSessionImpl::
    ~WebContentDecryptionModuleSessionImpl() {
  if (!session_id_.empty())
    adapter_->UnregisterSession(session_id_);
}

void WebContentDecryptionModuleSessionImpl::setClientInterface(Client* client) {
  client_ = client;
}

blink::WebString WebContentDecryptionModuleSessionImpl::sessionId() const {
  return blink::WebString::fromUTF8(session_id_);
}

void WebContentDecryptionModuleSessionImpl::initializeNewSession(
    blink::WebEncryptedMediaInitDataType init_data_type,
    const unsigned char* init_data,
    size_t init_data_length,
    blink::WebEncryptedMediaSessionType session_type,
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(session_id_.empty());

  // TODO(jrummell): |init_data_type| should be an enum all the way through
  // Chromium. http://crbug.com/417440
  std::string init_data_type_as_ascii = "unknown";
  switch (init_data_type) {
    case blink::WebEncryptedMediaInitDataType::Cenc:
      init_data_type_as_ascii = "cenc";
      break;
    case blink::WebEncryptedMediaInitDataType::Keyids:
      init_data_type_as_ascii = "keyids";
      break;
    case blink::WebEncryptedMediaInitDataType::Webm:
      init_data_type_as_ascii = "webm";
      break;
    case blink::WebEncryptedMediaInitDataType::Unknown:
      NOTREACHED() << "unexpected init_data_type";
      break;
  }

  // Step 5 from https://w3c.github.io/encrypted-media/#generateRequest.
  // 5. If the Key System implementation represented by this object's cdm
  //    implementation value does not support initDataType as an Initialization
  //    Data Type, return a promise rejected with a new DOMException whose name
  //    is NotSupportedError. String comparison is case-sensitive.
  if (!IsSupportedKeySystemWithInitDataType(adapter_->GetKeySystem(),
                                            init_data_type_as_ascii)) {
    std::string message = "The initialization data type " +
                          init_data_type_as_ascii +
                          " is not supported by the key system.";
    result.completeWithError(
        blink::WebContentDecryptionModuleExceptionNotSupportedError, 0,
        blink::WebString::fromUTF8(message));
    return;
  }

  MediaKeys::SessionType session_type_enum = MediaKeys::TEMPORARY_SESSION;
  switch (session_type) {
    case blink::WebEncryptedMediaSessionType::Temporary:
      session_type_enum = MediaKeys::TEMPORARY_SESSION;
      break;
    case blink::WebEncryptedMediaSessionType::PersistentLicense:
      session_type_enum = MediaKeys::PERSISTENT_LICENSE_SESSION;
      break;
    case blink::WebEncryptedMediaSessionType::PersistentReleaseMessage:
      session_type_enum = MediaKeys::PERSISTENT_RELEASE_MESSAGE_SESSION;
      break;
    case blink::WebEncryptedMediaSessionType::Unknown:
      NOTREACHED() << "unexpected session_type";
      break;
  }

  adapter_->InitializeNewSession(
      init_data_type_as_ascii, init_data,
      base::saturated_cast<int>(init_data_length), session_type_enum,
      scoped_ptr<NewSessionCdmPromise>(new NewSessionCdmResultPromise(
          result, adapter_->GetKeySystemUMAPrefix() + kGenerateRequestUMAName,
          base::Bind(
              &WebContentDecryptionModuleSessionImpl::OnSessionInitialized,
              base::Unretained(this)))));
}

// TODO(jrummell): Remove this. http://crbug.com/418239.
void WebContentDecryptionModuleSessionImpl::initializeNewSession(
    const blink::WebString& init_data_type,
    const uint8* init_data,
    size_t init_data_length,
    const blink::WebString& session_type,
    blink::WebContentDecryptionModuleResult result) {
  NOTREACHED();
}

void WebContentDecryptionModuleSessionImpl::load(
    const blink::WebString& session_id,
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(!session_id.isEmpty());
  DCHECK(session_id_.empty());

  // TODO(jrummell): Now that there are 2 types of persistent sessions, the
  // session type should be passed from blink. Type should also be passed in the
  // constructor (and removed from initializeNewSession()).
  adapter_->LoadSession(
      MediaKeys::PERSISTENT_LICENSE_SESSION, base::UTF16ToASCII(session_id),
      scoped_ptr<NewSessionCdmPromise>(new NewSessionCdmResultPromise(
          result, adapter_->GetKeySystemUMAPrefix() + kLoadSessionUMAName,
          base::Bind(
              &WebContentDecryptionModuleSessionImpl::OnSessionInitialized,
              base::Unretained(this)))));
}

void WebContentDecryptionModuleSessionImpl::update(
    const uint8* response,
    size_t response_length,
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(response);
  DCHECK(!session_id_.empty());
  adapter_->UpdateSession(
      session_id_, response, base::saturated_cast<int>(response_length),
      scoped_ptr<SimpleCdmPromise>(new CdmResultPromise<>(
          result, adapter_->GetKeySystemUMAPrefix() + kUpdateSessionUMAName)));
}

void WebContentDecryptionModuleSessionImpl::close(
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(!session_id_.empty());
  adapter_->CloseSession(
      session_id_,
      scoped_ptr<SimpleCdmPromise>(new CdmResultPromise<>(
          result, adapter_->GetKeySystemUMAPrefix() + kCloseSessionUMAName)));
}

void WebContentDecryptionModuleSessionImpl::remove(
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(!session_id_.empty());
  adapter_->RemoveSession(
      session_id_,
      scoped_ptr<SimpleCdmPromise>(new CdmResultPromise<>(
          result, adapter_->GetKeySystemUMAPrefix() + kRemoveSessionUMAName)));
}

void WebContentDecryptionModuleSessionImpl::OnSessionMessage(
    MediaKeys::MessageType message_type,
    const std::vector<uint8>& message) {
  DCHECK(client_) << "Client not set before message event";
  client_->message(convertMessageType(message_type),
                   message.empty() ? NULL : &message[0], message.size());
}

void WebContentDecryptionModuleSessionImpl::OnSessionKeysChange(
    bool has_additional_usable_key,
    CdmKeysInfo keys_info) {
  blink::WebVector<blink::WebEncryptedMediaKeyInformation> keys(
      keys_info.size());
  for (size_t i = 0; i < keys_info.size(); ++i) {
    const auto& key_info = keys_info[i];
    keys[i].setId(blink::WebData(reinterpret_cast<char*>(&key_info->key_id[0]),
                                 key_info->key_id.size()));
    keys[i].setStatus(convertStatus(key_info->status));
    keys[i].setSystemCode(key_info->system_code);
  }

  // Now send the event to blink.
  client_->keysStatusesChange(keys, has_additional_usable_key);
}

void WebContentDecryptionModuleSessionImpl::OnSessionExpirationUpdate(
    const base::Time& new_expiry_time) {
  client_->expirationChanged(new_expiry_time.ToJsTime());
}

void WebContentDecryptionModuleSessionImpl::OnSessionClosed() {
  if (is_closed_)
    return;

  is_closed_ = true;
  client_->close();
}

blink::WebContentDecryptionModuleResult::SessionStatus
WebContentDecryptionModuleSessionImpl::OnSessionInitialized(
    const std::string& session_id) {
  // CDM will return NULL if the session to be loaded can't be found.
  if (session_id.empty())
    return blink::WebContentDecryptionModuleResult::SessionNotFound;

  DCHECK(session_id_.empty()) << "Session ID may not be changed once set.";
  session_id_ = session_id;
  return adapter_->RegisterSession(session_id_, weak_ptr_factory_.GetWeakPtr())
             ? blink::WebContentDecryptionModuleResult::NewSession
             : blink::WebContentDecryptionModuleResult::SessionAlreadyExists;
}

}  // namespace media
