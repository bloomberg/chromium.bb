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

#include "base/crypto/rsa_private_key.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/nss_util_internal.h"
#include "base/nss_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"

using base::RSAPrivateKey;

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

  RSAPrivateKey* GenerateKeyPair();

  bool ExportPublicKeyViaDbus(RSAPrivateKey* pair);

  bool ExportPublicKeyToFile(RSAPrivateKey* pair, const FilePath& key_file);

  bool ImportPublicKey(const FilePath& key_file,
                       std::vector<uint8>* output);

  RSAPrivateKey* FindPrivateKey(const std::vector<uint8>& key);

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
  static const uint16 kKeySizeInBits;

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
const uint16 OwnerKeyUtilsImpl::kKeySizeInBits = 2048;

OwnerKeyUtilsImpl::OwnerKeyUtilsImpl() {
  // Ensure NSS is initialized.
  base::EnsureNSSInit();
}

OwnerKeyUtilsImpl::~OwnerKeyUtilsImpl() {}

RSAPrivateKey* OwnerKeyUtilsImpl::GenerateKeyPair() {
  return RSAPrivateKey::CreateSensitive(kKeySizeInBits);
}

bool OwnerKeyUtilsImpl::ExportPublicKeyViaDbus(RSAPrivateKey* pair) {
  DCHECK(pair);
  bool ok = false;

  std::vector<uint8> to_export;
  if (pair->ExportPublicKey(&to_export)) {
    LOG(ERROR) << "Formatting key for export failed!";
    return false;
  }

  // TODO(cmasone): send the data over dbus.
  return ok;
}

bool OwnerKeyUtilsImpl::ExportPublicKeyToFile(RSAPrivateKey* pair,
                                              const FilePath& key_file) {
  DCHECK(pair);
  bool ok = false;
  int safe_file_size = 0;

  std::vector<uint8> to_export;
  if (!pair->ExportPublicKey(&to_export)) {
    LOG(ERROR) << "Formatting key for export failed!";
    return false;
  }

  if (to_export.size() > static_cast<uint>(INT_MAX)) {
    LOG(ERROR) << "key is too big! " << to_export.size();
  } else {
    safe_file_size = static_cast<int>(to_export.size());

    ok = (safe_file_size ==
          file_util::WriteFile(key_file,
                               reinterpret_cast<char*>(&to_export.front()),
                               safe_file_size));
  }
  return ok;
}

bool OwnerKeyUtilsImpl::ImportPublicKey(const FilePath& key_file,
                                        std::vector<uint8>* output) {
  SECItem key_der;
  key_der.data = NULL;
  key_der.len = 0;

  if (!ReadDERFromFile(key_file, &key_der)) {
    PLOG(ERROR) << "Could not read in key from " << key_file.value() << ":";
    return false;
  }

  // See the comment in ExportPublicKey() for why I wrote a
  // SubjectPublicKeyInfo to disk instead of a key.
  CERTSubjectPublicKeyInfo* spki =
     SECKEY_DecodeDERSubjectPublicKeyInfo(&key_der);
  if (!spki) {
    LOG(ERROR) << "Could not decode key info: " << PR_GetError();
    SECITEM_FreeItem(&key_der, PR_FALSE);
    return false;
  }

  output->assign(reinterpret_cast<uint8*>(key_der.data),
                 reinterpret_cast<uint8*>(key_der.data + key_der.len));
  return key_der.len == output->size();
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

RSAPrivateKey* OwnerKeyUtilsImpl::FindPrivateKey(
    const std::vector<uint8>& key) {
  return RSAPrivateKey::FindFromPublicKeyInfo(key);
}

FilePath OwnerKeyUtilsImpl::GetOwnerKeyFilePath() {
  return FilePath(OwnerKeyUtilsImpl::kOwnerKeyFile);
}

}  // namespace chromeos
