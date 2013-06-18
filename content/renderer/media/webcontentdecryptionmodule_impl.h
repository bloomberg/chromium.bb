// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBCONTENTDECRYPTIONMODULE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBCONTENTDECRYPTIONMODULE_IMPL_H_

#include "base/strings/string16.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModule.h"

namespace content {

class WebContentDecryptionModuleImpl
    : public WebKit::WebContentDecryptionModule {
 public:
  static WebContentDecryptionModuleImpl* Create(const string16& key_system);

  virtual ~WebContentDecryptionModuleImpl();

  // WebKit::WebContentDecryptionModule implementation.
  virtual WebKit::WebContentDecryptionModuleSession* createSession(
      WebKit::WebContentDecryptionModuleSession::Client*);

 private:
  explicit WebContentDecryptionModuleImpl(const string16& key_system);

  DISALLOW_COPY_AND_ASSIGN(WebContentDecryptionModuleImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBCONTENTDECRYPTIONMODULE_IMPL_H_
