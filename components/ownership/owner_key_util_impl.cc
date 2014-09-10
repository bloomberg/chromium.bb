// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ownership/owner_key_util_impl.h"

#include <limits>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "crypto/rsa_private_key.h"

namespace ownership {

OwnerKeyUtilImpl::OwnerKeyUtilImpl(const base::FilePath& public_key_file)
    : public_key_file_(public_key_file) {
}

OwnerKeyUtilImpl::~OwnerKeyUtilImpl() {
}

bool OwnerKeyUtilImpl::ImportPublicKey(std::vector<uint8>* output) {
  // Get the file size (must fit in a 32 bit int for NSS).
  int64 file_size;
  if (!base::GetFileSize(public_key_file_, &file_size)) {
#if defined(OS_CHROMEOS)
    LOG(ERROR) << "Could not get size of " << public_key_file_.value();
#endif  // defined(OS_CHROMEOS)
    return false;
  }
  if (file_size > static_cast<int64>(std::numeric_limits<int>::max())) {
    LOG(ERROR) << public_key_file_.value() << "is " << file_size
               << "bytes!!!  Too big!";
    return false;
  }
  int32 safe_file_size = static_cast<int32>(file_size);

  output->resize(safe_file_size);

  if (safe_file_size == 0) {
    LOG(WARNING) << "Public key file is empty. This seems wrong.";
    return false;
  }

  // Get the key data off of disk
  int data_read =
      base::ReadFile(public_key_file_,
                     reinterpret_cast<char*>(vector_as_array(output)),
                     safe_file_size);
  return data_read == safe_file_size;
}

#if defined(USE_NSS)
crypto::RSAPrivateKey* OwnerKeyUtilImpl::FindPrivateKeyInSlot(
    const std::vector<uint8>& key,
    PK11SlotInfo* slot) {
  return crypto::RSAPrivateKey::FindFromPublicKeyInfoInSlot(key, slot);
}
#endif  // defined(USE_NSS)

bool OwnerKeyUtilImpl::IsPublicKeyPresent() {
  return base::PathExists(public_key_file_);
}

}  // namespace ownership
