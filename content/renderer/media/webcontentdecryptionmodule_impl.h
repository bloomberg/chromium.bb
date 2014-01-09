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
class Decryptor;
class MediaKeys;
}

namespace content {

class WebContentDecryptionModuleSessionImpl;
class SessionIdAdapter;

class WebContentDecryptionModuleImpl
    : public blink::WebContentDecryptionModule {
 public:
  static WebContentDecryptionModuleImpl* Create(
      const base::string16& key_system);

  virtual ~WebContentDecryptionModuleImpl();

  // Returns the Decryptor associated with this CDM. May be NULL if no
  // Decryptor associated with the MediaKeys object.
  // TODO(jrummell): Figure out lifetimes, as WMPI may still use the decryptor
  // after WebContentDecryptionModule is freed. http://crbug.com/330324
  media::Decryptor* GetDecryptor();

  // blink::WebContentDecryptionModule implementation.
  virtual blink::WebContentDecryptionModuleSession* createSession(
      blink::WebContentDecryptionModuleSession::Client* client);

 private:
  // Takes ownership of |media_keys| and |adapter|.
  WebContentDecryptionModuleImpl(scoped_ptr<media::MediaKeys> media_keys,
                                 scoped_ptr<SessionIdAdapter> adapter);

  // Called when a WebContentDecryptionModuleSessionImpl is closed.
  void OnSessionClosed(uint32 session_id);

  scoped_ptr<media::MediaKeys> media_keys_;
  scoped_ptr<SessionIdAdapter> adapter_;

  DISALLOW_COPY_AND_ASSIGN(WebContentDecryptionModuleImpl);
};

// Allow typecasting from blink type as this is the only implementation.
inline WebContentDecryptionModuleImpl* ToWebContentDecryptionModuleImpl(
    blink::WebContentDecryptionModule* cdm) {
  return static_cast<WebContentDecryptionModuleImpl*>(cdm);
}

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBCONTENTDECRYPTIONMODULE_IMPL_H_
