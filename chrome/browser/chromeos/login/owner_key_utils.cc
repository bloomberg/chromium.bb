// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/owner_key_utils.h"

#include <keyhi.h>     // SECKEY_CreateSubjectPublicKeyInfo()
#include <pk11pub.h>
#include <prerror.h>   // PR_GetError()
#include <secder.h>    // DER_Encode()
#include <secmod.h>

#include <limits>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/nss_util_internal.h"
#include "base/nss_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"

namespace chromeos {

///////////////////////////////////////////////////////////////////////////
// OwnerKeyUtils

// static
OwnerKeyUtils::Factory* OwnerKeyUtils::factory_ = NULL;

OwnerKeyUtils::OwnerKeyUtils() {}

OwnerKeyUtils::~OwnerKeyUtils() {}

///////////////////////////////////////////////////////////////////////////
// OwnerKeyUtilsImpl

class OwnerKeyUtilsImpl : public OwnerKeyUtils {
 public:
  OwnerKeyUtilsImpl();
  virtual ~OwnerKeyUtilsImpl();

  bool GenerateKeyPair(SECKEYPrivateKey** private_key_out,
                               SECKEYPublicKey** public_key_out);

  bool ExportPublicKeyViaDbus(SECKEYPublicKey* key);

  bool ExportPublicKeyToFile(SECKEYPublicKey* key, const FilePath& key_file);

  SECKEYPublicKey* ImportPublicKey(const FilePath& key_file);

  SECKEYPrivateKey* FindPrivateKey(SECKEYPublicKey* key);

  void DestroyKeys(SECKEYPrivateKey* private_key, SECKEYPublicKey* public_key);

  FilePath GetOwnerKeyFilePath();

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

  static bool EncodePublicKey(SECKEYPublicKey* key, std::string* out);

  // The file outside the owner's encrypted home directory where her
  // key will live.
  static const char kOwnerKeyFile[];

  // Key generation parameters.
  static const uint32 kKeyGenMechanism;  // used by PK11_GenerateKeyPair()
  static const unsigned long kExponent;
  static const int kKeySizeInBits;

  DISALLOW_COPY_AND_ASSIGN(OwnerKeyUtilsImpl);
};

// Defined here, instead of up above, because we need OwnerKeyUtilsImpl.
OwnerKeyUtils* OwnerKeyUtils::Create() {
  if (!factory_)
    return new OwnerKeyUtilsImpl();
  else
    return factory_->CreateOwnerKeyUtils();
}

// static
const char OwnerKeyUtilsImpl::kOwnerKeyFile[] = "/var/lib/whitelist/owner.key";

// We're generating and using 2048-bit RSA keys.
// static
const uint32 OwnerKeyUtilsImpl::kKeyGenMechanism = CKM_RSA_PKCS_KEY_PAIR_GEN;
// static
const unsigned long OwnerKeyUtilsImpl::kExponent = 65537UL;
// static
const int OwnerKeyUtilsImpl::kKeySizeInBits = 2048;

OwnerKeyUtilsImpl::OwnerKeyUtilsImpl(){}

OwnerKeyUtilsImpl::~OwnerKeyUtilsImpl() {}

bool OwnerKeyUtilsImpl::GenerateKeyPair(SECKEYPrivateKey** private_key_out,
                                        SECKEYPublicKey** public_key_out) {
  DCHECK(private_key_out);
  DCHECK(public_key_out);

  *private_key_out = NULL;
  *public_key_out = NULL;

  // Temporary structures used for generating the result
  // in the right format.
  PK11SlotInfo* slot = NULL;
  PK11RSAGenParams rsa_key_gen_params;
  void* key_gen_params;

  bool is_success = true;  // Set to false as soon as a step fails.

  rsa_key_gen_params.keySizeInBits = kKeySizeInBits;
  rsa_key_gen_params.pe = kExponent;
  key_gen_params = &rsa_key_gen_params;

  // Ensure NSS is initialized.
  base::EnsureNSSInit();

  slot = base::GetDefaultNSSKeySlot();
  if (!slot) {
    LOG(ERROR) << "Couldn't get Internal key slot!";
    is_success = false;
    goto failure;
  }

  // Need to make sure that the token was initialized.
  // Assume a null password.
  if (SECSuccess != PK11_Authenticate(slot, PR_TRUE, NULL)) {
    LOG(ERROR) << "Couldn't initialze PK11 token!";
    is_success = false;
    goto failure;
  }

  LOG(INFO) << "Creating key pair...";
  {
    base::AutoNSSWriteLock lock;
    *private_key_out = PK11_GenerateKeyPair(slot,
                                            kKeyGenMechanism,
                                            key_gen_params,
                                            public_key_out,
                                            PR_TRUE,  // isPermanent?
                                            PR_TRUE,  // isSensitive?
                                            NULL);
  }
  LOG(INFO) << "done.";

  if (!*private_key_out) {
    LOG(INFO) << "Generation of Keypair failed!";
    is_success = false;
    goto failure;
  }

 failure:
  if (!is_success) {
    LOG(ERROR) << "Owner key generation failed! (NSS error code "
               << PR_GetError() << ")";
    DestroyKeys(*private_key_out, *public_key_out);
    *private_key_out = NULL;
    *public_key_out = NULL;
  } else {
    LOG(INFO) << "Owner key generation succeeded!";
  }
  if (slot != NULL) {
    PK11_FreeSlot(slot);
  }

  return is_success;
}

bool OwnerKeyUtilsImpl::ExportPublicKeyViaDbus(SECKEYPublicKey* key) {
  DCHECK(key);
  bool ok = false;

  std::string to_export;
  if (!EncodePublicKey(key, &to_export)) {
    LOG(ERROR) << "Formatting key for export failed!";
    return ok;
  }

  // TODO(cmasone): send the data over dbus.
  return ok;
}

bool OwnerKeyUtilsImpl::ExportPublicKeyToFile(SECKEYPublicKey* key,
                                              const FilePath& key_file) {
  DCHECK(key);
  bool ok = false;
  int safe_file_size = 0;

  std::string to_export;
  if (!EncodePublicKey(key, &to_export)) {
    LOG(ERROR) << "Formatting key for export failed!";
    return ok;
  }

  if (to_export.length() > static_cast<uint>(INT_MAX)) {
    LOG(ERROR) << "key is too big! " << to_export.length();
  } else {
    safe_file_size = static_cast<int>(to_export.length());

    ok = (safe_file_size == file_util::WriteFile(key_file,
                                                 to_export.c_str(),
                                                 safe_file_size));
  }
  return ok;
}

SECKEYPublicKey* OwnerKeyUtilsImpl::ImportPublicKey(const FilePath& key_file) {
  SECItem key_der;
  key_der.data = NULL;
  key_der.len = 0;

  if (!ReadDERFromFile(key_file, &key_der)) {
    PLOG(ERROR) << "Could not read in key from " << key_file.value() << ":";
    return NULL;
  }

  // See the comment in ExportPublicKey() for why I wrote a
  // SubjectPublicKeyInfo to disk instead of a key.
  CERTSubjectPublicKeyInfo* spki =
     SECKEY_DecodeDERSubjectPublicKeyInfo(&key_der);
  if (!spki) {
    LOG(ERROR) << "Could not decode key info: " << PR_GetError();
    SECITEM_FreeItem(&key_der, PR_FALSE);
    return NULL;
  }

  SECKEYPublicKey *public_key = SECKEY_ExtractPublicKey(spki);
  SECKEY_DestroySubjectPublicKeyInfo(spki);
  SECITEM_FreeItem(&key_der, PR_FALSE);
  return public_key;
}

// static
bool OwnerKeyUtilsImpl::ReadDERFromFile(const FilePath& key_file,
                                        SECItem* key_der) {
  // I'd use NSS' SECU_ReadDERFromFile() here, but the SECU_* functions are
  // considered internal to the NSS command line utils.
  // This code is lifted, in spirit, from the implementation of that function.
  DCHECK(key_der) << "Don't pass NULL for |key_der|";
  DCHECK(key_der->data == NULL);
  DCHECK(key_der->len == 0);

  // Get the file size (must fit in a 32 bit int for NSS).
  int64 file_size;
  if (!file_util::GetFileSize(key_file, &file_size)) {
    LOG(ERROR) << "Could not get size of " << key_file.value();
    return false;
  }
  if (file_size > static_cast<int64>(std::numeric_limits<int>::max())) {
    LOG(ERROR) << key_file.value() << "is "
               << file_size << "bytes!!!  Too big!";
    return false;
  }
  int32 safe_file_size = static_cast<int32>(file_size);

  // Allocate space for the DER encoded data.
  key_der->data = NULL;
  if (!SECITEM_AllocItem(NULL, key_der, safe_file_size)) {
    LOG(ERROR) << "Could not create space for DER encoded key";
    return false;
  }

  // Get the key data off of disk
  int data_read = file_util::ReadFile(key_file,
                                      reinterpret_cast<char*>(key_der->data),
                                      safe_file_size);

  if (data_read != safe_file_size) {
    LOG(ERROR) << "Read the wrong amount of data from the DER encoded key! "
               << data_read;
    SECITEM_FreeItem(key_der, PR_FALSE);
    return false;
  }
  return true;
}

bool OwnerKeyUtilsImpl::EncodePublicKey(SECKEYPublicKey* key,
                                        std::string* out) {
  SECItem* der;

  // Instead of exporting/importing the key directly, I'm actually
  // going to use a SubjectPublicKeyInfo.  The reason is because NSS
  // exports functions that encode/decode these kinds of structures, while
  // it does not export the ones that deal directly with public keys.
  der = SECKEY_EncodeDERSubjectPublicKeyInfo(key);
  if (!der) {
    LOG(ERROR) << "Could not encode public key for export!";
    return false;
  }

  out->assign(reinterpret_cast<char*>(der->data), der->len);

  SECITEM_FreeItem(der, PR_TRUE);
  return true;
}

SECKEYPrivateKey* OwnerKeyUtilsImpl::FindPrivateKey(SECKEYPublicKey* key) {
  DCHECK(key);

  PK11SlotInfo* slot = NULL;
  SECItem* ck_id = NULL;
  SECKEYPrivateKey* found = NULL;

  slot = base::GetDefaultNSSKeySlot();
  if (!slot)
    goto cleanup;

  ck_id = PK11_MakeIDFromPubKey(&(key->u.rsa.modulus));
  if (!ck_id)
    goto cleanup;

  found = PK11_FindKeyByKeyID(slot, ck_id, NULL);

 cleanup:
  if (slot)
    PK11_FreeSlot(slot);
  if (ck_id)
    SECITEM_FreeItem(ck_id, PR_TRUE);

  return found;
}

void OwnerKeyUtilsImpl::DestroyKeys(SECKEYPrivateKey* private_key,
                                    SECKEYPublicKey* public_key) {
  base::AutoNSSWriteLock lock;
  if (private_key) {
    PK11_DestroyTokenObject(private_key->pkcs11Slot, private_key->pkcs11ID);
    SECKEY_DestroyPrivateKey(private_key);
  }
  if (public_key) {
    PK11_DestroyTokenObject(public_key->pkcs11Slot, public_key->pkcs11ID);
    SECKEY_DestroyPublicKey(public_key);
  }
}

FilePath OwnerKeyUtilsImpl::GetOwnerKeyFilePath() {
  return FilePath(OwnerKeyUtilsImpl::kOwnerKeyFile);
}

}  // namespace chromeos
