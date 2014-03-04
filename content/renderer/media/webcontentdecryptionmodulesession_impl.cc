// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webcontentdecryptionmodulesession_impl.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/renderer/media/cdm_session_adapter.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace content {

WebContentDecryptionModuleSessionImpl::WebContentDecryptionModuleSessionImpl(
    uint32 session_id,
    Client* client,
    const scoped_refptr<CdmSessionAdapter>& adapter)
    : adapter_(adapter),
      client_(client),
      session_id_(session_id) {
}

WebContentDecryptionModuleSessionImpl::
    ~WebContentDecryptionModuleSessionImpl() {
  adapter_->RemoveSession(session_id_);
}

blink::WebString WebContentDecryptionModuleSessionImpl::sessionId() const {
  return web_session_id_;
}

void WebContentDecryptionModuleSessionImpl::initializeNewSession(
    const blink::WebString& mime_type,
    const uint8* init_data, size_t init_data_length) {
  // TODO(ddorwin): Guard against this in supported types check and remove this.
  // Chromium only supports ASCII MIME types.
  if (!IsStringASCII(mime_type)) {
    NOTREACHED();
    OnSessionError(media::MediaKeys::kUnknownError, 0);
    return;
  }

  adapter_->InitializeNewSession(
      session_id_, UTF16ToASCII(mime_type), init_data, init_data_length);
}

void WebContentDecryptionModuleSessionImpl::update(const uint8* response,
                                                   size_t response_length) {
  DCHECK(response);
  adapter_->UpdateSession(session_id_, response, response_length);
}

void WebContentDecryptionModuleSessionImpl::release() {
  adapter_->ReleaseSession(session_id_);
}

void WebContentDecryptionModuleSessionImpl::OnSessionCreated(
    const std::string& web_session_id) {
  // Due to heartbeat messages, OnSessionCreated() can get called multiple
  // times.
  // TODO(jrummell): Once all CDMs are updated to support reference ids,
  // OnSessionCreated() should only be called once, and the second check can be
  // removed.
  blink::WebString id = blink::WebString::fromUTF8(web_session_id);
  DCHECK(web_session_id_.isEmpty() || web_session_id_ == id)
      << "Session ID may not be changed once set.";
  web_session_id_ = id;
}

void WebContentDecryptionModuleSessionImpl::OnSessionMessage(
    const std::vector<uint8>& message,
    const std::string& destination_url) {
  client_->message(message.empty() ? NULL : &message[0],
                   message.size(),
                   GURL(destination_url));
}

void WebContentDecryptionModuleSessionImpl::OnSessionReady() {
  client_->ready();
}

void WebContentDecryptionModuleSessionImpl::OnSessionClosed() {
  client_->close();
}

void WebContentDecryptionModuleSessionImpl::OnSessionError(
    media::MediaKeys::KeyError error_code,
    uint32 system_code) {
  client_->error(static_cast<Client::MediaKeyErrorCode>(error_code),
                 system_code);
}

}  // namespace content
