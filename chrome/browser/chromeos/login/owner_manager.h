// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_MANAGER_H_

#include "base/basictypes.h"

// Forward declarations of NSS data structures.
struct CERTCertificateStr;
struct CERTCertificateRequestStr;
struct SECKEYPrivateKeyStr;
struct SECKEYPublicKeyStr;
struct SECItemStr;

typedef struct CERTCertificateStr CERTCertificate;
typedef struct CERTCertificateRequestStr CERTCertificateRequest;
typedef struct SECKEYPrivateKeyStr SECKEYPrivateKey;
typedef struct SECKEYPublicKeyStr SECKEYPublicKey;
typedef struct SECItemStr SECItem;

class FilePath;

// This class allows the registration of an Owner of a Chromium OS device.
// It handles generating the appropriate keys and storing them in the
// appropriate locations.
class OwnerManager {
 public:
  OwnerManager() {}
  virtual ~OwnerManager() {}

  bool OwnershipAlreadyTaken();

  bool TakeOwnership();

  // Generate a public/private RSA keypair and store them in the NSS database.
  // The keys will be kKeySizeInBits in length (Recommend >= 2048 bits).
  //
  //  Returns false on error.
  //
  // The caller takes ownership of both objects, which are allocated by libnss.
  // To free them, call
  //    SECKEY_DestroyPrivateKey(*private_key_out);
  //    SECKEY_DestroyPublicKey(*public_key_out);
  static bool GenerateKeyPair(SECKEYPrivateKey** private_key_out,
                              SECKEYPublicKey** public_key_out);

  // DER encodes |key| and writes it out to |key_file|.
  // The blob on disk is a DER-encoded X509 SubjectPublicKeyInfo object.
  // Returns false on error.
  static bool ExportPublicKey(SECKEYPublicKey* key,
                              const FilePath& key_file);

  // Assumes that the file at |key_file| exists.
  // Caller takes ownership of returned object; returns NULL on error.
  // To free, call SECKEY_DestroyPublicKey.
  static SECKEYPublicKey* ImportPublicKey(const FilePath& key_file);

 private:
  // Fills in fields of |key_der| with DER encoded data from a file at
  // |key_file|.  The caller must pass in a pointer to an actual SECItem
  // struct for |key_der|.  |key_der->data| should be initialized to NULL
  // and |key_der->len| should be set to 0.
  //
  // Upon success, data is stored in key_der->data, and the caller takes
  // ownership.  Returns false on error.
  //
  // To free the data, call
  //    SECITEM_FreeItem(key_der, PR_FALSE);
  static bool ReadDERFromFile(const FilePath& key_file, SECItem* key_der);

  // The place outside the owner's encrypted home directory where her
  // key will live.
  static const char kOwnerKeyFile[];

  // Key generation parameters.
  static const uint32 kKeyGenMechanism;  // used by PK11_GenerateKeyPair()
  static const unsigned long kExponent;
  static const int kKeySizeInBits;
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OWNER_MANAGER_H_
