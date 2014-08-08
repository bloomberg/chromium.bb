// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_CRYPTO_VERIFY_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_CRYPTO_VERIFY_IMPL_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_service_client.h"

// Implementation of NetworkingPrivateServiceClient::CryptoVerify using
// networking_private_crypto.
class CryptoVerifyImpl
    : public extensions::NetworkingPrivateServiceClient::CryptoVerify {
 public:
  CryptoVerifyImpl();
  virtual ~CryptoVerifyImpl();

  // NetworkingPrivateServiceClient::CryptoVerify
  virtual void VerifyDestination(const Credentials& credentials,
                                 bool* verified,
                                 std::string* error) OVERRIDE;
  virtual void VerifyAndEncryptCredentials(
      const std::string& network_guid,
      const Credentials& credentials,
      const VerifyAndEncryptCredentialsCallback& callback) OVERRIDE;
  virtual void VerifyAndEncryptData(const Credentials& credentials,
                                    const std::string& data,
                                    std::string* base64_encoded_ciphertext,
                                    std::string* error) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptoVerifyImpl);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_CRYPTO_VERIFY_IMPL_H_
