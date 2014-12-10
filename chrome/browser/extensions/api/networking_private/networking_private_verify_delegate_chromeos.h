// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_VERIFY_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_VERIFY_DELEGATE_CHROMEOS_H_

#include "chrome/browser/extensions/api/networking_private/networking_private_delegate.h"

namespace extensions {

class NetworkingPrivateVerifyDelegateChromeOS
    : public NetworkingPrivateDelegate::VerifyDelegate {
 public:
  NetworkingPrivateVerifyDelegateChromeOS();
  ~NetworkingPrivateVerifyDelegateChromeOS() override;

  void VerifyDestination(const VerificationProperties& verification_properties,
                         const BoolCallback& success_callback,
                         const FailureCallback& failure_callback) override;
  void VerifyAndEncryptCredentials(
      const std::string& guid,
      const VerificationProperties& verification_properties,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) override;
  void VerifyAndEncryptData(
      const VerificationProperties& verification_properties,
      const std::string& data,
      const StringCallback& success_callback,
      const FailureCallback& failure_callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateVerifyDelegateChromeOS);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_VERIFY_DELEGATE_CHROMEOS_H_
