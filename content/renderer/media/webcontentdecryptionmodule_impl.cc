// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webcontentdecryptionmodule_impl.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/base/decrypt_config.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace content {

class WebContentDecryptionModuleSessionImpl
    : public WebKit::WebContentDecryptionModuleSession {
 public:
  explicit WebContentDecryptionModuleSessionImpl(
      WebKit::WebContentDecryptionModuleSession::Client* client);
  virtual ~WebContentDecryptionModuleSessionImpl();

  // WebKit::WebContentDecryptionModuleSession implementation.
  virtual WebKit::WebString sessionId() const { return session_id_; }
  virtual void generateKeyRequest(const WebKit::WebString& mime_type,
                                  const uint8* init_data,
                                  size_t init_data_length);
  virtual void update(const uint8* key, size_t key_length);
  virtual void close();

 private:
  WebKit::WebContentDecryptionModuleSession::Client* client_;
  WebKit::WebString session_id_;

  DISALLOW_COPY_AND_ASSIGN(WebContentDecryptionModuleSessionImpl);
};

WebContentDecryptionModuleSessionImpl::WebContentDecryptionModuleSessionImpl(
    WebKit::WebContentDecryptionModuleSession::Client* client)
    : client_(client) {
  // TODO(ddorwin): Populate session_id_ from the real implementation.
}

WebContentDecryptionModuleSessionImpl::
~WebContentDecryptionModuleSessionImpl() {
}

void WebContentDecryptionModuleSessionImpl::generateKeyRequest(
    const WebKit::WebString& mime_type,
    const uint8* init_data, size_t init_data_length) {
  // TODO(ddorwin): Call a real implementation and remove stub event triggers.
  NOTIMPLEMENTED();
  client_->keyMessage(NULL, 0, WebKit::WebURL());
}

void WebContentDecryptionModuleSessionImpl::update(const uint8* key,
                                                   size_t key_length) {
  DCHECK(key);

  // TODO(ddorwin): Call a real implementation and remove stub event triggers.
  NOTIMPLEMENTED();
  // TODO(ddorwin): Remove when we have a real implementation that passes tests.
  if (key_length !=
      static_cast<size_t>(media::DecryptConfig::kDecryptionKeySize)) {
    client_->keyError(
        WebKit::WebContentDecryptionModuleSession::Client::
        MediaKeyErrorCodeUnknown,
        0);
    return;
  }
  client_->keyAdded();
}

void WebContentDecryptionModuleSessionImpl::close() {
  // TODO(ddorwin): Call a real implementation.
  NOTIMPLEMENTED();
}

//------------------------------------------------------------------------------

WebContentDecryptionModuleImpl*
WebContentDecryptionModuleImpl::Create(const string16& key_system) {
  // TODO(ddorwin): Verify we can create the internal objects & load CDM first.
  return new WebContentDecryptionModuleImpl(key_system);
}

WebContentDecryptionModuleImpl::WebContentDecryptionModuleImpl(
    const string16& key_system) {
}

WebContentDecryptionModuleImpl::~WebContentDecryptionModuleImpl() {
}

WebKit::WebContentDecryptionModuleSession*
WebContentDecryptionModuleImpl::createSession(
    WebKit::WebContentDecryptionModuleSession::Client* client) {
  return new WebContentDecryptionModuleSessionImpl(client);
}

}  // namespace content
