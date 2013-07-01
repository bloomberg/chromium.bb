// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webcontentdecryptionmodulesession_impl.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace content {

WebContentDecryptionModuleSessionImpl::WebContentDecryptionModuleSessionImpl(
    media::MediaKeys* media_keys,
    Client* client,
    const SessionClosedCB& session_closed_cb)
    : media_keys_(media_keys),
      client_(client),
      session_closed_cb_(session_closed_cb) {
  DCHECK(media_keys_);
  // TODO(ddorwin): Populate session_id_ from the real implementation.
}

WebContentDecryptionModuleSessionImpl::
~WebContentDecryptionModuleSessionImpl() {
}

WebKit::WebString WebContentDecryptionModuleSessionImpl::sessionId() const {
  return WebKit::WebString::fromUTF8(session_id_);
}

void WebContentDecryptionModuleSessionImpl::generateKeyRequest(
    const WebKit::WebString& mime_type,
    const uint8* init_data, size_t init_data_length) {
  // TODO(ddorwin): Guard against this in supported types check and remove this.
  // Chromium only supports ASCII MIME types.
  if (!IsStringASCII(mime_type)) {
    NOTREACHED();
    KeyError(media::MediaKeys::kUnknownError, 0);
    return;
  }

  media_keys_->GenerateKeyRequest(UTF16ToASCII(mime_type),
                                  init_data, init_data_length);
}

void WebContentDecryptionModuleSessionImpl::update(const uint8* key,
                                                   size_t key_length) {
  DCHECK(key);
  media_keys_->AddKey(key, key_length, NULL, 0, session_id_);
}

void WebContentDecryptionModuleSessionImpl::close() {
  media_keys_->CancelKeyRequest(session_id_);

  // Detach from the CDM.
  if (!session_closed_cb_.is_null())
    base::ResetAndReturn(&session_closed_cb_).Run(session_id_);
}

void WebContentDecryptionModuleSessionImpl::KeyAdded() {
  client_->keyAdded();
}

void WebContentDecryptionModuleSessionImpl::KeyError(
    media::MediaKeys::KeyError error_code,
    int system_code) {
  client_->keyError(static_cast<Client::MediaKeyErrorCode>(error_code),
                    system_code);
}

void WebContentDecryptionModuleSessionImpl::KeyMessage(
    const std::vector<uint8>& message,
    const std::string& destination_url) {
  client_->keyMessage(message.empty() ? NULL : &message[0],
                      message.size(),
                      GURL(destination_url));
}

}  // namespace content
