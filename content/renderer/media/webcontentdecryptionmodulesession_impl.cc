// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webcontentdecryptionmodulesession_impl.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace content {

static const uint32 kStartingReferenceId = 1;
uint32 WebContentDecryptionModuleSessionImpl::next_reference_id_ =
    kStartingReferenceId;
COMPILE_ASSERT(kStartingReferenceId != media::MediaKeys::kInvalidReferenceId,
               invalid_starting_value);

WebContentDecryptionModuleSessionImpl::WebContentDecryptionModuleSessionImpl(
    media::MediaKeys* media_keys,
    Client* client,
    const SessionClosedCB& session_closed_cb)
    : media_keys_(media_keys),
      client_(client),
      session_closed_cb_(session_closed_cb),
      reference_id_(next_reference_id_++) {
  DCHECK(media_keys_);
}

WebContentDecryptionModuleSessionImpl::
~WebContentDecryptionModuleSessionImpl() {
}

blink::WebString WebContentDecryptionModuleSessionImpl::sessionId() const {
  return blink::WebString::fromUTF8(session_id_);
}

void WebContentDecryptionModuleSessionImpl::generateKeyRequest(
    const blink::WebString& mime_type,
    const uint8* init_data, size_t init_data_length) {
  // TODO(ddorwin): Guard against this in supported types check and remove this.
  // Chromium only supports ASCII MIME types.
  if (!IsStringASCII(mime_type)) {
    NOTREACHED();
    KeyError(media::MediaKeys::kUnknownError, 0);
    return;
  }

  media_keys_->GenerateKeyRequest(
      reference_id_, UTF16ToASCII(mime_type), init_data, init_data_length);
}

void WebContentDecryptionModuleSessionImpl::update(const uint8* key,
                                                   size_t key_length) {
  DCHECK(key);
  media_keys_->AddKey(reference_id_, key, key_length, NULL, 0);
}

void WebContentDecryptionModuleSessionImpl::close() {
  // Detach from the CDM.
  // TODO(jrummell): We shouldn't detach here because closed and other events
  // may be fired in the latest version of the spec. http://crbug.com/309235
  if (!session_closed_cb_.is_null())
    base::ResetAndReturn(&session_closed_cb_).Run(reference_id_);
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

void WebContentDecryptionModuleSessionImpl::SetSessionId(
    const std::string& session_id) {
  // Due to heartbeat messages, SetSessionId() can get called multiple times.
  // TODO(jrummell): Once all CDMs are updated to support reference ids,
  // SetSessionId() should only be called once, and the second check can be
  // removed.
  DCHECK(session_id_.empty() || session_id_ == session_id)
      << "Session ID may not be changed once set.";
  session_id_ = session_id;
}

}  // namespace content
