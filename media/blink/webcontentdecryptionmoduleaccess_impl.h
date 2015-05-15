// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBCONTENTDECRYPTIONMODULEACCESS_IMPL_H_
#define MEDIA_BLINK_WEBCONTENTDECRYPTIONMODULEACCESS_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleAccess.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleResult.h"
#include "third_party/WebKit/public/platform/WebMediaKeySystemConfiguration.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace media {

class WebEncryptedMediaClientImpl;

class WebContentDecryptionModuleAccessImpl
    : public blink::WebContentDecryptionModuleAccess {
 public:
  static WebContentDecryptionModuleAccessImpl* Create(
      const blink::WebString& key_system,
      const blink::WebMediaKeySystemConfiguration& configuration,
      const blink::WebSecurityOrigin& security_origin,
      const base::WeakPtr<WebEncryptedMediaClientImpl>& client);
  virtual ~WebContentDecryptionModuleAccessImpl();

  // blink::WebContentDecryptionModuleAccess interface.
  virtual blink::WebMediaKeySystemConfiguration getConfiguration();
  virtual void createContentDecryptionModule(
      blink::WebContentDecryptionModuleResult result);

 private:
  WebContentDecryptionModuleAccessImpl(
      const blink::WebString& key_system,
      const blink::WebMediaKeySystemConfiguration& configuration,
      const blink::WebSecurityOrigin& security_origin,
      const base::WeakPtr<WebEncryptedMediaClientImpl>& client);

  blink::WebString key_system_;
  blink::WebMediaKeySystemConfiguration configuration_;
  blink::WebSecurityOrigin security_origin_;

  // Keep a WeakPtr as client is owned by render_frame_impl.
  base::WeakPtr<WebEncryptedMediaClientImpl> client_;

  DISALLOW_COPY_AND_ASSIGN(WebContentDecryptionModuleAccessImpl);
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBCONTENTDECRYPTIONMODULEACCESS_IMPL_H_
