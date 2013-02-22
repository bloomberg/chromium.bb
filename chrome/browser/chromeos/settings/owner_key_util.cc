// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/owner_key_util.h"

#include <limits>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "chrome/common/chrome_paths.h"
#include "crypto/rsa_private_key.h"

namespace chromeos {

///////////////////////////////////////////////////////////////////////////
// OwnerKeyUtil

OwnerKeyUtil* OwnerKeyUtil::Create() {
  base::FilePath owner_key_path;
  CHECK(PathService::Get(chrome::FILE_OWNER_KEY, &owner_key_path));
  return new OwnerKeyUtilImpl(owner_key_path);
}

OwnerKeyUtil::OwnerKeyUtil() {}

OwnerKeyUtil::~OwnerKeyUtil() {}

///////////////////////////////////////////////////////////////////////////
// OwnerKeyUtilImpl

OwnerKeyUtilImpl::OwnerKeyUtilImpl(const base::FilePath& key_file)
    : key_file_(key_file) {}

OwnerKeyUtilImpl::~OwnerKeyUtilImpl() {}

bool OwnerKeyUtilImpl::ImportPublicKey(std::vector<uint8>* output) {
  // Get the file size (must fit in a 32 bit int for NSS).
  int64 file_size;
  if (!file_util::GetFileSize(key_file_, &file_size)) {
    LOG(ERROR) << "Could not get size of " << key_file_.value();
    return false;
  }
  if (file_size > static_cast<int64>(std::numeric_limits<int>::max())) {
    LOG(ERROR) << key_file_.value() << "is "
               << file_size << "bytes!!!  Too big!";
    return false;
  }
  int32 safe_file_size = static_cast<int32>(file_size);

  output->resize(safe_file_size);

  if (safe_file_size == 0) {
    LOG(WARNING) << "Public key file is empty. This seems wrong.";
    return false;
  }

  // Get the key data off of disk
  int data_read = file_util::ReadFile(
      key_file_,
      reinterpret_cast<char*>(vector_as_array(output)),
      safe_file_size);
  return data_read == safe_file_size;
}

crypto::RSAPrivateKey* OwnerKeyUtilImpl::FindPrivateKey(
    const std::vector<uint8>& key) {
  return crypto::RSAPrivateKey::FindFromPublicKeyInfo(key);
}

bool OwnerKeyUtilImpl::IsPublicKeyPresent() {
  return file_util::PathExists(key_file_);
}

}  // namespace chromeos
