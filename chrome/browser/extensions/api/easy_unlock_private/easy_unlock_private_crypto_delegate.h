// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CRYPTO_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CRYPTO_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/api/easy_unlock_private.h"

namespace extensions {
namespace api {

// Wrapper around EasyUnlock dbus client on ChromeOS. On other platforms, the
// methods are stubbed out.
class EasyUnlockPrivateCryptoDelegate {
 public:
  typedef base::Callback<void(const std::string& data)> DataCallback;

  typedef base::Callback<void(const std::string& public_key,
                              const std::string& private_key)>
      KeyPairCallback;

  virtual ~EasyUnlockPrivateCryptoDelegate() {}

  // Creates platform specific delegate instance. Non Chrome OS implementations
  // currently do nothing but invoke callbacks with empty data.
  static scoped_ptr<EasyUnlockPrivateCryptoDelegate> Create();

  // See chromeos/dbus/easy_unlock_client.h for info on these methods.
  virtual void GenerateEcP256KeyPair(const KeyPairCallback& callback) = 0;
  virtual void PerformECDHKeyAgreement(const std::string& private_key,
                                       const std::string& public_key,
                                       const DataCallback& callback) = 0;
  virtual void CreateSecureMessage(
      const std::string& payload,
      const std::string& secret_key,
      const std::string& associated_data,
      const std::string& public_metadata,
      const std::string& verification_key_id,
      easy_unlock_private::EncryptionType encryption_type,
      easy_unlock_private::SignatureType signature_type,
      const DataCallback& callback) = 0;
  virtual void UnwrapSecureMessage(
      const std::string& message,
      const std::string& secret_key,
      const std::string& associated_data,
      easy_unlock_private::EncryptionType encryption_type,
      easy_unlock_private::SignatureType signature_type,
      const DataCallback& callback) = 0;
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_CRYPTO_DELEGATE_H_
