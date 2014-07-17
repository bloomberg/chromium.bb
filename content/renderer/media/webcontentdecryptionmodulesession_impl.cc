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
#include "third_party/WebKit/public/platform/WebURL.h"

namespace content {

// For backwards compatibility with blink not using
// WebContentDecryptionModuleResult, reserve an index for |outstanding_results_|
// that will not be used when adding a WebContentDecryptionModuleResult.
// TODO(jrummell): Remove once blink always uses
// WebContentDecryptionModuleResult.
const uint32 kReservedIndex = 0;

static blink::WebContentDecryptionModuleException ConvertException(
    media::MediaKeys::Exception exception_code) {
  switch (exception_code) {
    case media::MediaKeys::NOT_SUPPORTED_ERROR:
      return blink::WebContentDecryptionModuleExceptionNotSupportedError;
    case media::MediaKeys::INVALID_STATE_ERROR:
      return blink::WebContentDecryptionModuleExceptionInvalidStateError;
    case media::MediaKeys::INVALID_ACCESS_ERROR:
      return blink::WebContentDecryptionModuleExceptionInvalidAccessError;
    case media::MediaKeys::QUOTA_EXCEEDED_ERROR:
      return blink::WebContentDecryptionModuleExceptionQuotaExceededError;
    case media::MediaKeys::UNKNOWN_ERROR:
      return blink::WebContentDecryptionModuleExceptionUnknownError;
    case media::MediaKeys::CLIENT_ERROR:
      return blink::WebContentDecryptionModuleExceptionClientError;
    case media::MediaKeys::OUTPUT_ERROR:
      return blink::WebContentDecryptionModuleExceptionOutputError;
    default:
      NOTREACHED();
      return blink::WebContentDecryptionModuleExceptionUnknownError;
  }
}

WebContentDecryptionModuleSessionImpl::WebContentDecryptionModuleSessionImpl(
    const scoped_refptr<CdmSessionAdapter>& adapter)
    : adapter_(adapter),
      is_closed_(false),
      next_available_result_index_(1),
      weak_ptr_factory_(this) {
}

WebContentDecryptionModuleSessionImpl::
    ~WebContentDecryptionModuleSessionImpl() {
  if (!web_session_id_.empty())
    adapter_->RemoveSession(web_session_id_);

  // Release any WebContentDecryptionModuleResult objects that are left. Their
  // index will have been passed down via a CdmPromise, but it uses a WeakPtr.
  DLOG_IF(WARNING, outstanding_results_.size() > 0)
      << "Clearing " << outstanding_results_.size() << " results";
  for (ResultMap::iterator it = outstanding_results_.begin();
       it != outstanding_results_.end();
       ++it) {
    it->second.completeWithError(
        blink::WebContentDecryptionModuleExceptionInvalidStateError,
        0,
        "Outstanding request being cancelled.");
  }
  outstanding_results_.clear();
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
  // TODO(ddorwin): Guard against this in supported types check and remove this.
  // Chromium only supports ASCII MIME types.
  if (!base::IsStringASCII(init_data_type)) {
    NOTREACHED();
    OnSessionError(media::MediaKeys::NOT_SUPPORTED_ERROR,
                   0,
                   "The initialization data type " + init_data_type.utf8() +
                       " is not supported by the key system.");
    return;
  }

  std::string init_data_type_as_ascii = base::UTF16ToASCII(init_data_type);
  DLOG_IF(WARNING, init_data_type_as_ascii.find('/') != std::string::npos)
      << "init_data_type '" << init_data_type_as_ascii
      << "' may be a MIME type";

  scoped_ptr<media::NewSessionCdmPromise> promise(
      new media::NewSessionCdmPromise(
          base::Bind(&WebContentDecryptionModuleSessionImpl::SessionCreated,
                     weak_ptr_factory_.GetWeakPtr(),
                     kReservedIndex),
          base::Bind(&WebContentDecryptionModuleSessionImpl::OnSessionError,
                     weak_ptr_factory_.GetWeakPtr())));
  adapter_->InitializeNewSession(init_data_type_as_ascii,
                                 init_data,
                                 init_data_length,
                                 media::MediaKeys::TEMPORARY_SESSION,
                                 promise.Pass());
}

void WebContentDecryptionModuleSessionImpl::update(const uint8* response,
                                                   size_t response_length) {
  DCHECK(response);
  scoped_ptr<media::SimpleCdmPromise> promise(new media::SimpleCdmPromise(
      base::Bind(&WebContentDecryptionModuleSessionImpl::OnSessionReady,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&WebContentDecryptionModuleSessionImpl::OnSessionError,
                 weak_ptr_factory_.GetWeakPtr())));
  adapter_->UpdateSession(
      web_session_id_, response, response_length, promise.Pass());
}

void WebContentDecryptionModuleSessionImpl::release() {
  scoped_ptr<media::SimpleCdmPromise> promise(new media::SimpleCdmPromise(
      base::Bind(&WebContentDecryptionModuleSessionImpl::OnSessionClosed,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&WebContentDecryptionModuleSessionImpl::OnSessionError,
                 weak_ptr_factory_.GetWeakPtr())));
  adapter_->ReleaseSession(web_session_id_, promise.Pass());
}

void WebContentDecryptionModuleSessionImpl::initializeNewSession(
    const blink::WebString& init_data_type,
    const uint8* init_data,
    size_t init_data_length,
    const blink::WebString& session_type,
    blink::WebContentDecryptionModuleResult result) {
  uint32 result_index = AddResult(result);

  // TODO(ddorwin): Guard against this in supported types check and remove this.
  // Chromium only supports ASCII MIME types.
  if (!base::IsStringASCII(init_data_type)) {
    NOTREACHED();
    SessionError(result_index,
                 media::MediaKeys::NOT_SUPPORTED_ERROR,
                 0,
                 "The initialization data type " + init_data_type.utf8() +
                     " is not supported by the key system.");
    return;
  }

  std::string init_data_type_as_ascii = base::UTF16ToASCII(init_data_type);
  DLOG_IF(WARNING, init_data_type_as_ascii.find('/') != std::string::npos)
      << "init_data_type '" << init_data_type_as_ascii
      << "' may be a MIME type";

  scoped_ptr<media::NewSessionCdmPromise> promise(
      new media::NewSessionCdmPromise(
          base::Bind(&WebContentDecryptionModuleSessionImpl::SessionCreated,
                     weak_ptr_factory_.GetWeakPtr(),
                     result_index),
          base::Bind(&WebContentDecryptionModuleSessionImpl::SessionError,
                     weak_ptr_factory_.GetWeakPtr(),
                     result_index)));
  adapter_->InitializeNewSession(init_data_type_as_ascii,
                                 init_data,
                                 init_data_length,
                                 media::MediaKeys::TEMPORARY_SESSION,
                                 promise.Pass());
}

void WebContentDecryptionModuleSessionImpl::update(
    const uint8* response,
    size_t response_length,
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(response);
  uint32 result_index = AddResult(result);
  scoped_ptr<media::SimpleCdmPromise> promise(new media::SimpleCdmPromise(
      base::Bind(
          &WebContentDecryptionModuleSessionImpl::SessionUpdatedOrReleased,
          weak_ptr_factory_.GetWeakPtr(),
          result_index),
      base::Bind(&WebContentDecryptionModuleSessionImpl::SessionError,
                 weak_ptr_factory_.GetWeakPtr(),
                 result_index)));
  adapter_->UpdateSession(
      web_session_id_, response, response_length, promise.Pass());
}

void WebContentDecryptionModuleSessionImpl::release(
    blink::WebContentDecryptionModuleResult result) {
  uint32 result_index = AddResult(result);
  scoped_ptr<media::SimpleCdmPromise> promise(new media::SimpleCdmPromise(
      base::Bind(
          &WebContentDecryptionModuleSessionImpl::SessionUpdatedOrReleased,
          weak_ptr_factory_.GetWeakPtr(),
          result_index),
      base::Bind(&WebContentDecryptionModuleSessionImpl::SessionError,
                 weak_ptr_factory_.GetWeakPtr(),
                 result_index)));
  adapter_->ReleaseSession(web_session_id_, promise.Pass());
}

void WebContentDecryptionModuleSessionImpl::OnSessionMessage(
    const std::vector<uint8>& message,
    const GURL& destination_url) {
  DCHECK(client_) << "Client not set before message event";
  client_->message(
      message.empty() ? NULL : &message[0], message.size(), destination_url);
}

void WebContentDecryptionModuleSessionImpl::OnSessionReady() {
  client_->ready();
}

void WebContentDecryptionModuleSessionImpl::OnSessionClosed() {
  if (!is_closed_) {
    is_closed_ = true;
    client_->close();
  }
}

void WebContentDecryptionModuleSessionImpl::OnSessionError(
    media::MediaKeys::Exception exception_code,
    uint32 system_code,
    const std::string& error_message) {
  // Convert |exception_code| back to MediaKeyErrorCode if possible.
  // TODO(jrummell): Update this conversion when promises flow
  // back into blink:: (as blink:: will have its own error definition).
  switch (exception_code) {
    case media::MediaKeys::CLIENT_ERROR:
      client_->error(Client::MediaKeyErrorCodeClient, system_code);
      break;
    default:
      // This will include all other CDM4 errors and any error generated
      // by CDM5 or later.
      client_->error(Client::MediaKeyErrorCodeUnknown, system_code);
      break;
  }
}

void WebContentDecryptionModuleSessionImpl::SessionCreated(
    uint32 result_index,
    const std::string& web_session_id) {
  blink::WebContentDecryptionModuleResult::SessionStatus status;

  // CDM will return NULL if the session to be loaded can't be found.
  if (web_session_id.empty()) {
    status = blink::WebContentDecryptionModuleResult::SessionNotFound;
  } else {
    DCHECK(web_session_id_.empty())
        << "Session ID may not be changed once set.";
    web_session_id_ = web_session_id;
    status =
        adapter_->RegisterSession(web_session_id_,
                                  weak_ptr_factory_.GetWeakPtr())
            ? blink::WebContentDecryptionModuleResult::NewSession
            : blink::WebContentDecryptionModuleResult::SessionAlreadyExists;
  }

  ResultMap::iterator it = outstanding_results_.find(result_index);
  if (it != outstanding_results_.end()) {
    blink::WebContentDecryptionModuleResult& result = it->second;
    result.completeWithSession(status);
    outstanding_results_.erase(result_index);
  }
}

void WebContentDecryptionModuleSessionImpl::SessionUpdatedOrReleased(
    uint32 result_index) {
  ResultMap::iterator it = outstanding_results_.find(result_index);
  DCHECK(it != outstanding_results_.end());
  blink::WebContentDecryptionModuleResult& result = it->second;
  result.complete();
  outstanding_results_.erase(it);
}

void WebContentDecryptionModuleSessionImpl::SessionError(
    uint32 result_index,
    media::MediaKeys::Exception exception_code,
    uint32 system_code,
    const std::string& error_message) {
  ResultMap::iterator it = outstanding_results_.find(result_index);
  DCHECK(it != outstanding_results_.end());
  blink::WebContentDecryptionModuleResult& result = it->second;
  result.completeWithError(ConvertException(exception_code),
                           system_code,
                           blink::WebString::fromUTF8(error_message));
  outstanding_results_.erase(it);
}

uint32 WebContentDecryptionModuleSessionImpl::AddResult(
    blink::WebContentDecryptionModuleResult result) {
  uint32 result_index = next_available_result_index_++;
  DCHECK(result_index != kReservedIndex);
  outstanding_results_.insert(std::make_pair(result_index, result));
  return result_index;
}

}  // namespace content
