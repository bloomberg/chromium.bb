// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webcontentdecryptionmoduleaccess_impl.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleResult.h"
#include "webcontentdecryptionmodule_impl.h"

namespace media {

// The caller owns the created cdm (passed back using |result|).
static void CreateCdm(CdmFactory* cdm_factory,
                      blink::WebSecurityOrigin security_origin,
                      blink::WebString key_system,
                      blink::WebContentDecryptionModuleResult result) {
  WebContentDecryptionModuleImpl::Create(cdm_factory, security_origin,
                                         key_system, result);
}

WebContentDecryptionModuleAccessImpl*
WebContentDecryptionModuleAccessImpl::Create(
    const blink::WebString& key_system,
    const blink::WebSecurityOrigin& security_origin,
    CdmFactory* cdm_factory) {
  return new WebContentDecryptionModuleAccessImpl(key_system, security_origin,
                                                  cdm_factory);
}

WebContentDecryptionModuleAccessImpl::WebContentDecryptionModuleAccessImpl(
    const blink::WebString& key_system,
    const blink::WebSecurityOrigin& security_origin,
    CdmFactory* cdm_factory)
    : key_system_(key_system),
      security_origin_(security_origin),
      cdm_factory_(cdm_factory) {
}

WebContentDecryptionModuleAccessImpl::~WebContentDecryptionModuleAccessImpl() {
}

void WebContentDecryptionModuleAccessImpl::createContentDecryptionModule(
    blink::WebContentDecryptionModuleResult result) {
  // This method needs to run asynchronously, as it may need to load the CDM.
  // As this object's lifetime is controlled by MediaKeySystemAccess on the
  // blink side, copy all values needed by CreateCdm() in case the blink object
  // gets garbage-collected.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(&CreateCdm, cdm_factory_, security_origin_,
                            key_system_, result));
}

}  // namespace media
