// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBCONTENTDECRYPTIONMODULEACCESS_IMPL_H_
#define MEDIA_BLINK_WEBCONTENTDECRYPTIONMODULEACCESS_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/cdm_factory.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleAccess.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"

namespace blink {
class WebLocalFrame;
}

namespace media {

class WebContentDecryptionModuleAccessImpl
    : public blink::WebContentDecryptionModuleAccess {
 public:
  static WebContentDecryptionModuleAccessImpl* Create(
      const blink::WebString& key_system,
      const blink::WebSecurityOrigin& security_origin,
      CdmFactory* cdm_factory);
  virtual ~WebContentDecryptionModuleAccessImpl();

  // blink::WebContentDecryptionModuleAccess interface.
  virtual void createContentDecryptionModule(
      blink::WebContentDecryptionModuleResult result) override;

 private:
  WebContentDecryptionModuleAccessImpl(
      const blink::WebString& key_system,
      const blink::WebSecurityOrigin& security_origin,
      CdmFactory* cdm_factory);

  DISALLOW_COPY_AND_ASSIGN(WebContentDecryptionModuleAccessImpl);

  blink::WebString key_system_;
  blink::WebSecurityOrigin security_origin_;
  CdmFactory* cdm_factory_;
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBCONTENTDECRYPTIONMODULEACCESS_IMPL_H_
