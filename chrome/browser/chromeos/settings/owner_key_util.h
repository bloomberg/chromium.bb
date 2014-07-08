// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_OWNER_KEY_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_OWNER_KEY_UTIL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/x509_util_nss.h"

namespace base {
class FilePath;
}

namespace crypto {
class RSAPrivateKey;
}

namespace chromeos {

class OwnerKeyUtilTest;

class PublicKey : public base::RefCountedThreadSafe<PublicKey> {
 public:
  PublicKey();

  std::vector<uint8>& data() { return data_; }

  bool is_loaded() const { return !data_.empty(); }

  std::string as_string() {
    return std::string(reinterpret_cast<const char*>(vector_as_array(&data_)),
                       data_.size());
  }

 private:
  friend class base::RefCountedThreadSafe<PublicKey>;

  virtual ~PublicKey();

  std::vector<uint8> data_;

  DISALLOW_COPY_AND_ASSIGN(PublicKey);
};

class PrivateKey : public base::RefCountedThreadSafe<PrivateKey> {
 public:
  explicit PrivateKey(crypto::RSAPrivateKey* key);

  crypto::RSAPrivateKey* key() { return key_.get(); }

 private:
  friend class base::RefCountedThreadSafe<PrivateKey>;

  virtual ~PrivateKey();

  scoped_ptr<crypto::RSAPrivateKey> key_;

  DISALLOW_COPY_AND_ASSIGN(PrivateKey);
};

class OwnerKeyUtil : public base::RefCountedThreadSafe<OwnerKeyUtil> {
 public:
  // Creates an OwnerKeyUtil instance.
  static OwnerKeyUtil* Create();

  // Attempts to read the public key from the file system.
  // Upon success, returns true and populates |output|.  False on failure.
  virtual bool ImportPublicKey(std::vector<uint8>* output) = 0;

  // Looks for the private key associated with |key| in the |slot|
  // and returns it if it can be found.  Returns NULL otherwise.
  // Caller takes ownership.
  virtual crypto::RSAPrivateKey* FindPrivateKeyInSlot(
      const std::vector<uint8>& key,
      PK11SlotInfo* slot) = 0;

  // Checks whether the public key is present in the file system.
  virtual bool IsPublicKeyPresent() = 0;

 protected:
  OwnerKeyUtil();
  virtual ~OwnerKeyUtil();

 private:
  friend class base::RefCountedThreadSafe<OwnerKeyUtil>;

  FRIEND_TEST_ALL_PREFIXES(OwnerKeyUtilTest, ExportImportPublicKey);
};

// Implementation of OwnerKeyUtil that is used in production code.
class OwnerKeyUtilImpl : public OwnerKeyUtil {
 public:
  explicit OwnerKeyUtilImpl(const base::FilePath& public_key_file);

  // OwnerKeyUtil:
  virtual bool ImportPublicKey(std::vector<uint8>* output) OVERRIDE;
  virtual crypto::RSAPrivateKey* FindPrivateKeyInSlot(
      const std::vector<uint8>& key,
      PK11SlotInfo* slot) OVERRIDE;
  virtual bool IsPublicKeyPresent() OVERRIDE;

 protected:
  virtual ~OwnerKeyUtilImpl();

 private:
  // The file that holds the public key.
  base::FilePath key_file_;

  DISALLOW_COPY_AND_ASSIGN(OwnerKeyUtilImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_OWNER_KEY_UTIL_H_
