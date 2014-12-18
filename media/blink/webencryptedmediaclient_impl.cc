// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webencryptedmediaclient_impl.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebEncryptedMediaRequest.h"
#include "third_party/WebKit/public/platform/WebMediaKeySystemConfiguration.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "webcontentdecryptionmoduleaccess_impl.h"

namespace media {

static bool IsKeySystemSupportedWithInitDataType(
    const blink::WebString& keySystem,
    const blink::WebString& initDataType) {
  DCHECK(!keySystem.isEmpty());
  return true;
}

WebEncryptedMediaClientImpl::WebEncryptedMediaClientImpl(
    scoped_ptr<CdmFactory> cdm_factory)
    : cdm_factory_(cdm_factory.Pass()) {
}

WebEncryptedMediaClientImpl::~WebEncryptedMediaClientImpl() {
}

void WebEncryptedMediaClientImpl::requestMediaKeySystemAccess(
    blink::WebEncryptedMediaRequest request) {
  // TODO(jrummell): This should be asynchronous.

  // Continued from requestMediaKeySystemAccess(), step 5, from
  // https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#requestmediakeysystemaccess
  //
  // 5.1 If keySystem is not supported or not allowed in the origin of the
  //     calling context's Document, return a promise rejected with a new
  //     DOMException whose name is NotSupportedError.
  //     (Handled by Chromium.)

  // 5.2 If supportedConfigurations was not provided, resolve the promise
  //     with a new MediaKeySystemAccess object, execute the following steps:
  size_t number_configs = request.supportedConfigurations().size();
  if (!number_configs) {
    // 5.2.1 Let access be a new MediaKeySystemAccess object, and initialize
    //       it as follows:
    // 5.2.1.1 Set the keySystem attribute to keySystem.
    // 5.2.2 Resolve promise with access and abort these steps.
    request.requestSucceeded(WebContentDecryptionModuleAccessImpl::Create(
        request.keySystem(), request.securityOrigin(), cdm_factory_.get()));
    return;
  }

  // 5.3 For each element of supportedConfigurations:
  // 5.3.1 Let combination be the element.
  // 5.3.2 For each dictionary member in combination:
  for (size_t i = 0; i < number_configs; ++i) {
    const auto& combination = request.supportedConfigurations()[i];
    // 5.3.2.1 If the member's value cannot be satisfied together in
    //         combination with the previous members, continue to the next
    //         iteration of the loop.
    // 5.3.3 If keySystem is supported and allowed in the origin of the
    //       calling context's Document in the configuration specified by
    //       the combination of the values in combination, execute the
    //       following steps:
    // FIXME: This test needs to be enhanced to use more values from
    //        combination.
    for (size_t j = 0; j < combination.initDataTypes.size(); ++j) {
      const auto& initDataType = combination.initDataTypes[j];
      if (IsKeySystemSupportedWithInitDataType(request.keySystem(),
                                               initDataType)) {
        // 5.3.3.1 Let access be a new MediaKeySystemAccess object, and
        //         initialize it as follows:
        // 5.3.3.1.1 Set the keySystem attribute to keySystem.
        // 5.3.3.2 Resolve promise with access and abort these steps.
        request.requestSucceeded(WebContentDecryptionModuleAccessImpl::Create(
            request.keySystem(), request.securityOrigin(), cdm_factory_.get()));
        return;
      }
    }
  }

  // 5.4 Reject promise with a new DOMException whose name is
  //     NotSupportedError.
  request.requestNotSupported(
      "There were no supported combinations in supportedConfigurations.");
}

}  // namespace media
