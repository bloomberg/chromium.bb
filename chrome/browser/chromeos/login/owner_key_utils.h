// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_KEY_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_KEY_UTILS_H_

#include "base/basictypes.h"

// Forward declarations of NSS data structures.
struct SECKEYPrivateKeyStr;
struct SECKEYPublicKeyStr;
struct SECItemStr;

typedef struct SECKEYPrivateKeyStr SECKEYPrivateKey;
typedef struct SECKEYPublicKeyStr SECKEYPublicKey;
typedef struct SECItemStr SECItem;

class FilePath;

class OwnerKeyUtils {
 public:
  class Factory {
   public:
    virtual OwnerKeyUtils* CreateOwnerKeyUtils() = 0;
  };

  OwnerKeyUtils();
  virtual ~OwnerKeyUtils();

  // Sets the factory used by the static method Create to create an
  // OwnerKeyUtils.  OwnerKeyUtils does not take ownership of
  // |factory|. A value of NULL results in an OwnerKeyUtils being
  // created directly.
#if defined(UNIT_TEST)
  static void set_factory(Factory* factory) { factory_ = factory; }
#endif

  // Creates an OwnerKeyUtils, ownership returns to the caller. If there is no
  // Factory (the default) this creates and returns a new OwnerKeyUtils.
  static OwnerKeyUtils* Create();

  // Generate a public/private RSA keypair and store them in the NSS database.
  // The keys will be kKeySizeInBits in length (Recommend >= 2048 bits).
  //
  //  Returns false on error.
  //
  // The caller takes ownership of both objects, which are allocated by libnss.
  // To free them, call
  //    SECKEY_DestroyPrivateKey(*private_key_out);
  //    SECKEY_DestroyPublicKey(*public_key_out);
  virtual bool GenerateKeyPair(SECKEYPrivateKey** private_key_out,
                               SECKEYPublicKey** public_key_out) = 0;

  // DER encodes |key| and writes it out to |key_file|.
  // The blob on disk is a DER-encoded X509 SubjectPublicKeyInfo object.
  // Returns false on error.
  virtual bool ExportPublicKey(SECKEYPublicKey* key,
                               const FilePath& key_file) = 0;

  // Assumes that the file at |key_file| exists.
  // Caller takes ownership of returned object; returns NULL on error.
  // To free, call SECKEY_DestroyPublicKey.
  virtual SECKEYPublicKey* ImportPublicKey(const FilePath& key_file) = 0;

 private:
  static Factory* factory_;
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_KEY_UTILS_H_
