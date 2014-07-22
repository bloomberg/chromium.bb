// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_

#include <string>

#include "base/basictypes.h"
#include "extensions/browser/extension_function.h"

// Implementations for chrome.easyUnlockPrivate API functions.

namespace extensions {
namespace api {

class EasyUnlockPrivatePerformECDHKeyAgreementFunction
    : public AsyncExtensionFunction {
 public:
  EasyUnlockPrivatePerformECDHKeyAgreementFunction();

 protected:
  virtual ~EasyUnlockPrivatePerformECDHKeyAgreementFunction();

  virtual bool RunAsync() OVERRIDE;

 private:
  void OnData(const std::string& secret_key);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.performECDHKeyAgreement",
                             EASYUNLOCKPRIVATE_PERFORMECDHKEYAGREEMENT)
};

class EasyUnlockPrivateGenerateEcP256KeyPairFunction
    : public AsyncExtensionFunction {
 public:
  EasyUnlockPrivateGenerateEcP256KeyPairFunction();

 protected:
  virtual ~EasyUnlockPrivateGenerateEcP256KeyPairFunction();

  virtual bool RunAsync() OVERRIDE;

 private:
  void OnData(const std::string& public_key,
              const std::string& private_key);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.generateEcP256KeyPair",
                             EASYUNLOCKPRIVATE_GENERATEECP256KEYPAIR)
};

class EasyUnlockPrivateCreateSecureMessageFunction
    : public AsyncExtensionFunction {
 public:
  EasyUnlockPrivateCreateSecureMessageFunction();

 protected:
  virtual ~EasyUnlockPrivateCreateSecureMessageFunction();

  virtual bool RunAsync() OVERRIDE;

 private:
  void OnData(const std::string& message);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.createSecureMessage",
                             EASYUNLOCKPRIVATE_CREATESECUREMESSAGE)
};

class EasyUnlockPrivateUnwrapSecureMessageFunction
    : public AsyncExtensionFunction {
 public:
  EasyUnlockPrivateUnwrapSecureMessageFunction();

 protected:
  virtual ~EasyUnlockPrivateUnwrapSecureMessageFunction();

  virtual bool RunAsync() OVERRIDE;

 private:
  void OnData(const std::string& data);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.unwrapSecureMessage",
                             EASYUNLOCKPRIVATE_UNWRAPSECUREMESSAGE)
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_
