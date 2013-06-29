// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBCONTENTDECRYPTIONMODULESESSION_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBCONTENTDECRYPTIONMODULESESSION_IMPL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "media/base/media_keys.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleSession.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace media {
class MediaKeys;
}

namespace content {

class WebContentDecryptionModuleSessionImpl
    : public WebKit::WebContentDecryptionModuleSession {
 public:
  typedef base::Callback<void(const std::string& session_id)> SessionClosedCB;

  WebContentDecryptionModuleSessionImpl(
      media::MediaKeys* media_keys,
      Client* client,
      const SessionClosedCB& session_closed_cb);
  virtual ~WebContentDecryptionModuleSessionImpl();

  // WebKit::WebContentDecryptionModuleSession implementation.
  virtual WebKit::WebString sessionId() const OVERRIDE;
  virtual void generateKeyRequest(const WebKit::WebString& mime_type,
                                  const uint8* init_data,
                                  size_t init_data_length) OVERRIDE;
  virtual void update(const uint8* key, size_t key_length) OVERRIDE;
  virtual void close() OVERRIDE;

  const std::string& session_id() const { return session_id_; }

  // Callbacks.
  void KeyAdded();
  void KeyError(media::MediaKeys::KeyError error_code, int system_code);
  void KeyMessage(const std::vector<uint8>& message,
                  const std::string& destination_url);

 private:
  // Non-owned pointers.
  media::MediaKeys* media_keys_;
  Client* client_;

  SessionClosedCB session_closed_cb_;

  std::string session_id_;

  DISALLOW_COPY_AND_ASSIGN(WebContentDecryptionModuleSessionImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBCONTENTDECRYPTIONMODULESESSION_IMPL_H_
