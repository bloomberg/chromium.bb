// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OWNERSHIP_OWNER_KEY_UTIL_H_
#define COMPONENTS_OWNERSHIP_OWNER_KEY_UTIL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "components/ownership/ownership_export.h"

#if defined(USE_NSS)
struct PK11SlotInfoStr;
typedef struct PK11SlotInfoStr PK11SlotInfo;
#endif  // defined(USE_NSS)

namespace crypto {
class RSAPrivateKey;
}

namespace ownership {

class OwnerKeyUtilTest;

// This class is a ref-counted wrapper around a plain public key.
class OWNERSHIP_EXPORT PublicKey
    : public base::RefCountedThreadSafe<PublicKey> {
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

// This class is a ref-counted wrapper around a crypto::RSAPrivateKey
// instance.
class OWNERSHIP_EXPORT PrivateKey
    : public base::RefCountedThreadSafe<PrivateKey> {
 public:
  explicit PrivateKey(crypto::RSAPrivateKey* key);

  crypto::RSAPrivateKey* key() { return key_.get(); }

 private:
  friend class base::RefCountedThreadSafe<PrivateKey>;

  virtual ~PrivateKey();

  scoped_ptr<crypto::RSAPrivateKey> key_;

  DISALLOW_COPY_AND_ASSIGN(PrivateKey);
};

// This class is a helper class that allows to import public/private
// parts of the owner key.
class OWNERSHIP_EXPORT OwnerKeyUtil
    : public base::RefCountedThreadSafe<OwnerKeyUtil> {
 public:
  // Attempts to read the public key from the file system.  Upon success,
  // returns true and populates |output|.  False on failure.
  virtual bool ImportPublicKey(std::vector<uint8>* output) = 0;

#if defined(USE_NSS)
  // Looks for the private key associated with |key| in the |slot|
  // and returns it if it can be found.  Returns NULL otherwise.
  // Caller takes ownership.
  virtual crypto::RSAPrivateKey* FindPrivateKeyInSlot(
      const std::vector<uint8>& key,
      PK11SlotInfo* slot) = 0;
#endif  // defined(USE_NSS)

  // Checks whether the public key is present in the file system.
  virtual bool IsPublicKeyPresent() = 0;

 protected:
  virtual ~OwnerKeyUtil() {}

 private:
  friend class base::RefCountedThreadSafe<OwnerKeyUtil>;
};

}  // namespace ownership

#endif  // COMPONENTS_OWNERSHIP_OWNER_KEY_UTIL_H_
