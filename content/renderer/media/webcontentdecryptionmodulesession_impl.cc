// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webcontentdecryptionmodulesession_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/cdm_session_adapter.h"
#include "media/base/cdm_promise.h"
#include "media/base/media_keys.h"
#include "media/blink/cdm_result_promise.h"
#include "media/blink/new_session_cdm_result_promise.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace content {

const char kCreateSessionUMAName[] = "CreateSession";
const char kLoadSessionUMAName[] = "LoadSession";

WebContentDecryptionModuleSessionImpl::WebContentDecryptionModuleSessionImpl(
    const scoped_refptr<CdmSessionAdapter>& adapter)
    : adapter_(adapter),
      is_closed_(false),
      weak_ptr_factory_(this) {
}

WebContentDecryptionModuleSessionImpl::
    ~WebContentDecryptionModuleSessionImpl() {
  if (!web_session_id_.empty())
    adapter_->UnregisterSession(web_session_id_);
}

void WebContentDecryptionModuleSessionImpl::setClientInterface(Client* client) {
  client_ = client;
}

blink::WebString WebContentDecryptionModuleSessionImpl::sessionId() const {
  return blink::WebString::fromUTF8(web_session_id_);
}

void WebContentDecryptionModuleSessionImpl::initializeNewSession(
    const blink::WebString& init_data_type,
    const uint8* init_data,
    size_t init_data_length) {
  // TODO(jrummell): Remove once blink updated.
  NOTREACHED();
}

void WebContentDecryptionModuleSessionImpl::update(const uint8* response,
                                                   size_t response_length) {
  // TODO(jrummell): Remove once blink updated.
  NOTREACHED();
}

void WebContentDecryptionModuleSessionImpl::release() {
  // TODO(jrummell): Remove once blink updated.
  NOTREACHED();
}

void WebContentDecryptionModuleSessionImpl::initializeNewSession(
    const blink::WebString& init_data_type,
    const uint8* init_data,
    size_t init_data_length,
    const blink::WebString& session_type,
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(web_session_id_.empty());

  // TODO(ddorwin): Guard against this in supported types check and remove this.
  // Chromium only supports ASCII MIME types.
  if (!base::IsStringASCII(init_data_type)) {
    NOTREACHED();
    std::string message = "The initialization data type " +
                          init_data_type.utf8() +
                          " is not supported by the key system.";
    result.completeWithError(
        blink::WebContentDecryptionModuleExceptionNotSupportedError,
        0,
        blink::WebString::fromUTF8(message));
    return;
  }

  std::string init_data_type_as_ascii = base::UTF16ToASCII(init_data_type);
  DLOG_IF(WARNING, init_data_type_as_ascii.find('/') != std::string::npos)
      << "init_data_type '" << init_data_type_as_ascii
      << "' may be a MIME type";

  adapter_->InitializeNewSession(
      init_data_type_as_ascii,
      init_data,
      init_data_length,
      media::MediaKeys::TEMPORARY_SESSION,
      scoped_ptr<media::NewSessionCdmPromise>(
          new media::NewSessionCdmResultPromise(
              result,
              adapter_->GetKeySystemUMAPrefix() + kCreateSessionUMAName,
              base::Bind(
                  &WebContentDecryptionModuleSessionImpl::OnSessionInitialized,
                  base::Unretained(this)))));
}

void WebContentDecryptionModuleSessionImpl::load(
    const blink::WebString& session_id,
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(!session_id.isEmpty());
  DCHECK(web_session_id_.empty());

  adapter_->LoadSession(
      base::UTF16ToASCII(session_id),
      scoped_ptr<media::NewSessionCdmPromise>(
          new media::NewSessionCdmResultPromise(
              result,
              adapter_->GetKeySystemUMAPrefix() + kLoadSessionUMAName,
              base::Bind(
                  &WebContentDecryptionModuleSessionImpl::OnSessionInitialized,
                  base::Unretained(this)))));
}

void WebContentDecryptionModuleSessionImpl::update(
    const uint8* response,
    size_t response_length,
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(response);
  DCHECK(!web_session_id_.empty());
  adapter_->UpdateSession(
      web_session_id_,
      response,
      response_length,
      scoped_ptr<media::SimpleCdmPromise>(
          new media::CdmResultPromise<>(result, std::string())));
}

void WebContentDecryptionModuleSessionImpl::close(
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(!web_session_id_.empty());
  adapter_->CloseSession(
      web_session_id_,
      scoped_ptr<media::SimpleCdmPromise>(
          new media::CdmResultPromise<>(result, std::string())));
}

void WebContentDecryptionModuleSessionImpl::remove(
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(!web_session_id_.empty());
  adapter_->RemoveSession(
      web_session_id_,
      scoped_ptr<media::SimpleCdmPromise>(
          new media::CdmResultPromise<>(result, std::string())));
}

void WebContentDecryptionModuleSessionImpl::getUsableKeyIds(
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(!web_session_id_.empty());
  adapter_->GetUsableKeyIds(
      web_session_id_,
      scoped_ptr<media::KeyIdsPromise>(
          new media::CdmResultPromise<media::KeyIdsVector>(result,
                                                           std::string())));
}

void WebContentDecryptionModuleSessionImpl::release(
    blink::WebContentDecryptionModuleResult result) {
  close(result);
}

void WebContentDecryptionModuleSessionImpl::OnSessionMessage(
    const std::vector<uint8>& message,
    const GURL& destination_url) {
  DCHECK(client_) << "Client not set before message event";
  client_->message(
      message.empty() ? NULL : &message[0], message.size(), destination_url);
}

void WebContentDecryptionModuleSessionImpl::OnSessionKeysChange(
    bool has_additional_usable_key) {
  // TODO(jrummell): Update this once Blink client supports this.
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
    const std::string& web_session_id) {
  // CDM will return NULL if the session to be loaded can't be found.
  if (web_session_id.empty())
    return blink::WebContentDecryptionModuleResult::SessionNotFound;

  DCHECK(web_session_id_.empty()) << "Session ID may not be changed once set.";
  web_session_id_ = web_session_id;
  return adapter_->RegisterSession(web_session_id_,
                                   weak_ptr_factory_.GetWeakPtr())
             ? blink::WebContentDecryptionModuleResult::NewSession
             : blink::WebContentDecryptionModuleResult::SessionAlreadyExists;
}

}  // namespace content
