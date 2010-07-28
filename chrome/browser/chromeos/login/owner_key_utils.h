// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_KEY_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_KEY_UTILS_H_
#pragma once

#include "base/basictypes.h"

// Forward declarations of NSS data structures.
struct SECKEYPrivateKeyStr;
struct SECKEYPublicKeyStr;
struct SECItemStr;

typedef struct SECKEYPrivateKeyStr SECKEYPrivateKey;
typedef struct SECKEYPublicKeyStr SECKEYPublicKey;
typedef struct SECItemStr SECItem;

class FilePath;

namespace chromeos {

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

  // DER encodes |key| and exports it via DBus.
  // The data sent is a DER-encoded X509 SubjectPublicKeyInfo object.
  // Returns false on error.
  virtual bool ExportPublicKeyViaDbus(SECKEYPublicKey* key) = 0;

  // DER encodes |key| and writes it out to |key_file|.
  // The blob on disk is a DER-encoded X509 SubjectPublicKeyInfo object.
  // Returns false on error.
  virtual bool ExportPublicKeyToFile(SECKEYPublicKey* key,
                                     const FilePath& key_file) = 0;

  // Assumes that the file at |key_file| exists.
  // Caller takes ownership of returned object; returns NULL on error.
  // To free, call SECKEY_DestroyPublicKey.
  virtual SECKEYPublicKey* ImportPublicKey(const FilePath& key_file) = 0;


  // Looks for the private key associated with |key| in the default slot,
  // and returns it if it can be found.  Returns NULL otherwise.
  // To free, call SECKEY_DestroyPrivateKey.
  virtual SECKEYPrivateKey* FindPrivateKey(SECKEYPublicKey* key) = 0;

  // If something's gone wrong with key generation or key exporting, the
  // caller may wish to nuke some keys.  This will destroy key objects in
  // memory and ALSO remove them from the NSS database.
  virtual void DestroyKeys(SECKEYPrivateKey* private_key,
                           SECKEYPublicKey* public_key) = 0;

  virtual FilePath GetOwnerKeyFilePath() = 0;

 private:
  static Factory* factory_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_KEY_UTILS_H_
