// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webcontentdecryptionmoduleaccess_impl.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/blink/webencryptedmediaclient_impl.h"

namespace media {

// The caller owns the created cdm (passed back using |result|).
static void CreateCdm(const base::WeakPtr<WebEncryptedMediaClientImpl>& client,
                      const blink::WebString& key_system,
                      bool allow_distinctive_identifier,
                      bool allow_persistent_state,
                      const blink::WebSecurityOrigin& security_origin,
                      blink::WebContentDecryptionModuleResult result) {
  // If |client| is gone (due to the frame getting destroyed), it is
  // impossible to create the CDM, so fail.
  if (!client) {
    result.completeWithError(
        blink::WebContentDecryptionModuleExceptionInvalidStateError, 0,
        "Failed to create CDM.");
    return;
  }

  client->CreateCdm(key_system, allow_distinctive_identifier,
                    allow_persistent_state, security_origin, result);
}

WebContentDecryptionModuleAccessImpl*
WebContentDecryptionModuleAccessImpl::Create(
    const blink::WebString& key_system,
    const blink::WebMediaKeySystemConfiguration& configuration,
    const blink::WebSecurityOrigin& security_origin,
    const base::WeakPtr<WebEncryptedMediaClientImpl>& client) {
  return new WebContentDecryptionModuleAccessImpl(key_system, configuration,
                                                  security_origin, client);
}

WebContentDecryptionModuleAccessImpl::WebContentDecryptionModuleAccessImpl(
    const blink::WebString& key_system,
    const blink::WebMediaKeySystemConfiguration& configuration,
    const blink::WebSecurityOrigin& security_origin,
    const base::WeakPtr<WebEncryptedMediaClientImpl>& client)
    : key_system_(key_system),
      configuration_(configuration),
      security_origin_(security_origin),
      client_(client) {
}

WebContentDecryptionModuleAccessImpl::~WebContentDecryptionModuleAccessImpl() {
}

blink::WebMediaKeySystemConfiguration
WebContentDecryptionModuleAccessImpl::getConfiguration() {
  return configuration_;
}

void WebContentDecryptionModuleAccessImpl::createContentDecryptionModule(
    blink::WebContentDecryptionModuleResult result) {
  // Convert the accumulated configuration requirements to bools. Accumulated
  // configurations never have optional requirements.
  DCHECK(configuration_.distinctiveIdentifier !=
         blink::WebMediaKeySystemConfiguration::Requirement::Optional);
  DCHECK(configuration_.persistentState !=
         blink::WebMediaKeySystemConfiguration::Requirement::Optional);
  bool allow_distinctive_identifier =
      (configuration_.distinctiveIdentifier ==
       blink::WebMediaKeySystemConfiguration::Requirement::Required);
  bool allow_persistent_state =
      (configuration_.persistentState ==
       blink::WebMediaKeySystemConfiguration::Requirement::Required);

  // This method needs to run asynchronously, as it may need to load the CDM.
  // As this object's lifetime is controlled by MediaKeySystemAccess on the
  // blink side, copy all values needed by CreateCdm() in case the blink object
  // gets garbage-collected.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&CreateCdm, client_, key_system_, allow_distinctive_identifier,
                 allow_persistent_state, security_origin_, result));
}

}  // namespace media
