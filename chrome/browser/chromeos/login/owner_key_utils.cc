// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/owner_key_utils.h"

#include <limits>

#include "base/crypto/rsa_private_key.h"
#include "base/crypto/signature_creator.h"
#include "base/crypto/signature_verifier.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/common/extensions/extension_constants.h"

using base::RSAPrivateKey;
using extension_misc::kSignatureAlgorithm;

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

  bool ImportPublicKey(const FilePath& key_file,
                       std::vector<uint8>* output);

  bool Verify(const std::string& data,
              const std::vector<uint8> signature,
              const std::vector<uint8> public_key);

  bool Sign(const std::string& data,
            std::vector<uint8>* OUT_signature,
            base::RSAPrivateKey* key);

  RSAPrivateKey* FindPrivateKey(const std::vector<uint8>& key);

  FilePath GetOwnerKeyFilePath();

 protected:
  virtual ~OwnerKeyUtilsImpl();

  bool ExportPublicKeyToFile(RSAPrivateKey* pair, const FilePath& key_file);

 private:
  // The file outside the owner's encrypted home directory where her
  // key will live.
  static const char kOwnerKeyFile[];

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

OwnerKeyUtilsImpl::OwnerKeyUtilsImpl() {}

OwnerKeyUtilsImpl::~OwnerKeyUtilsImpl() {}

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

  output->resize(safe_file_size);
  // Get the key data off of disk
  int data_read = file_util::ReadFile(key_file,
                                      reinterpret_cast<char*>(&(output->at(0))),
                                      safe_file_size);
  return data_read == safe_file_size;
}

bool OwnerKeyUtilsImpl::Verify(const std::string& data,
                               const std::vector<uint8> signature,
                               const std::vector<uint8> public_key) {
  base::SignatureVerifier verifier;
  if (!verifier.VerifyInit(kSignatureAlgorithm, sizeof(kSignatureAlgorithm),
                           &signature[0], signature.size(),
                           &public_key[0], public_key.size())) {
    return false;
  }

  verifier.VerifyUpdate(reinterpret_cast<const uint8*>(data.c_str()),
                        data.length());
  return (verifier.VerifyFinal());
}

bool OwnerKeyUtilsImpl::Sign(const std::string& data,
                             std::vector<uint8>* OUT_signature,
                             base::RSAPrivateKey* key) {
  scoped_ptr<base::SignatureCreator> signer(
      base::SignatureCreator::Create(key));
  if (!signer->Update(reinterpret_cast<const uint8*>(data.c_str()),
                      data.length())) {
    return false;
  }
  return signer->Final(OUT_signature);
}

RSAPrivateKey* OwnerKeyUtilsImpl::FindPrivateKey(
    const std::vector<uint8>& key) {
  return RSAPrivateKey::FindFromPublicKeyInfo(key);
}

FilePath OwnerKeyUtilsImpl::GetOwnerKeyFilePath() {
  return FilePath(OwnerKeyUtilsImpl::kOwnerKeyFile);
}

}  // namespace chromeos
