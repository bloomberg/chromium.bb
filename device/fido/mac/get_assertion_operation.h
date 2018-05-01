// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_GET_ASSERTION_OPERATION_H_
#define DEVICE_FIDO_MAC_GET_ASSERTION_OPERATION_H_

#import <LocalAuthentication/LocalAuthentication.h>
#import <Security/Security.h>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/mac/availability.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/fido_constants.h"

namespace device {
namespace fido {
namespace mac {

// GetAssertionOperation implements the authenticatorGetAssertion operation. The
// operation can be invoked via its |Run| method, which must only be called
// once.
//
// It prompts the user for consent via Touch ID, then looks up a key pair
// matching the request in the keychain and generates an assertion.
//
// For documentation on the keychain item metadata, see
// |MakeCredentialOperation|.
class API_AVAILABLE(macosx(10.12.2))
    COMPONENT_EXPORT(DEVICE_FIDO) GetAssertionOperation {
 public:
  using Callback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      base::Optional<AuthenticatorGetAssertionResponse>)>;

  GetAssertionOperation(CtapGetAssertionRequest request,
                        std::string profile_id,
                        std::string keychain_access_group,
                        Callback callback);
  GetAssertionOperation(GetAssertionOperation&&);
  GetAssertionOperation& operator=(GetAssertionOperation&&);
  ~GetAssertionOperation();

  void Run();

 private:
  // Callback for `LAContext evaluateAccessControl`. Any NSError that gets
  // passed is autoreleased.
  void PromptTouchIdDone(bool success, NSError* err);

  // profile_id_ identifies the user profile from which the request originates.
  // TODO(martinkr): Figure out how to generate and store this (PrefService?).
  std::string profile_id_;
  std::string keychain_access_group_;

  CtapGetAssertionRequest request_;
  Callback callback_;

  base::scoped_nsobject<LAContext> context_;
  base::ScopedCFTypeRef<SecAccessControlRef> access_;
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_GET_ASSERTION_OPERATION_H_
