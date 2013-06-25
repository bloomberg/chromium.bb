// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBCONTENTDECRYPTIONMODULE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBCONTENTDECRYPTIONMODULE_IMPL_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModule.h"

namespace media {
class MediaKeys;
}

namespace content {

class WebContentDecryptionModuleSessionImpl;
class SessionIdAdapter;

class WebContentDecryptionModuleImpl
    : public WebKit::WebContentDecryptionModule {
 public:
  static WebContentDecryptionModuleImpl* Create(const string16& key_system);

  virtual ~WebContentDecryptionModuleImpl();

  // WebKit::WebContentDecryptionModule implementation.
  virtual WebKit::WebContentDecryptionModuleSession* createSession(
      WebKit::WebContentDecryptionModuleSession::Client* client);

 private:
  // Takes ownership of |media_keys| and |adapter|.
  WebContentDecryptionModuleImpl(scoped_ptr<media::MediaKeys> media_keys,
                                 scoped_ptr<SessionIdAdapter> adapter);

  // Called when a WebContentDecryptionModuleSessionImpl is closed.
  void OnSessionClosed(const std::string& session_id);

  scoped_ptr<media::MediaKeys> media_keys_;
  scoped_ptr<SessionIdAdapter> adapter_;

  DISALLOW_COPY_AND_ASSIGN(WebContentDecryptionModuleImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBCONTENTDECRYPTIONMODULE_IMPL_H_
