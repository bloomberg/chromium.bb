// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_MOCK_OWNER_KEY_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_MOCK_OWNER_KEY_UTIL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/settings/owner_key_util.h"

namespace chromeos {

class MockOwnerKeyUtil : public OwnerKeyUtil {
 public:
  MockOwnerKeyUtil();

  // OwnerKeyUtil:
  virtual bool ImportPublicKey(std::vector<uint8>* output) OVERRIDE;
  virtual crypto::RSAPrivateKey* FindPrivateKey(
      const std::vector<uint8>& key) OVERRIDE;
  virtual bool IsPublicKeyPresent() OVERRIDE;

  // Clears the public and private keys.
  void Clear();

  // Configures the mock to return the given public key.
  void SetPublicKey(const std::vector<uint8>& key);

  // Sets the public key to use from the given private key, but doesn't
  // configure the private key.
  void SetPublicKeyFromPrivateKey(crypto::RSAPrivateKey* key);

  // Sets the private key (also configures the public key).
  void SetPrivateKey(crypto::RSAPrivateKey* key);

 protected:
  virtual ~MockOwnerKeyUtil();

 private:
  std::vector<uint8> public_key_;
  scoped_ptr<crypto::RSAPrivateKey> private_key_;

  DISALLOW_COPY_AND_ASSIGN(MockOwnerKeyUtil);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_MOCK_OWNER_KEY_UTIL_H_
