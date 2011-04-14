// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/owner_key_utils.h"

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_temp_dir.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/rsa_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class OwnerKeyUtilsTest : public ::testing::Test {
 public:
  OwnerKeyUtilsTest() : utils_(OwnerKeyUtils::Create()) {}
  virtual ~OwnerKeyUtilsTest() {}

  virtual void SetUp() {
    crypto::OpenPersistentNSSDB();
  }

  // Key generation parameters.
  static const uint16 kKeySizeInBits;

  scoped_refptr<OwnerKeyUtils> utils_;
};

// We're generating and using 2048-bit RSA keys.
// static
const uint16 OwnerKeyUtilsTest::kKeySizeInBits = 2048;

TEST_F(OwnerKeyUtilsTest, ExportImportPublicKey) {
  scoped_ptr<crypto::RSAPrivateKey> pair(
      crypto::RSAPrivateKey::CreateSensitive(kKeySizeInBits));
  ASSERT_NE(pair.get(), reinterpret_cast<crypto::RSAPrivateKey*>(NULL));

  // Export public key to file.
  ScopedTempDir tmpdir;
  FilePath tmpfile;
  ASSERT_TRUE(tmpdir.CreateUniqueTempDir());
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(tmpdir.path(), &tmpfile));
  ASSERT_TRUE(utils_->ExportPublicKeyToFile(pair.get(), tmpfile));

  // Export public key, so that we can compare it to the one we get off disk.
  std::vector<uint8> public_key;
  ASSERT_TRUE(pair->ExportPublicKey(&public_key));
  std::vector<uint8> from_disk;
  ASSERT_TRUE(utils_->ImportPublicKey(tmpfile, &from_disk));

  std::vector<uint8>::iterator pubkey_it;
  std::vector<uint8>::iterator disk_it;
  for (pubkey_it = public_key.begin(), disk_it = from_disk.begin();
       pubkey_it < public_key.end();
       pubkey_it++, disk_it++) {
    EXPECT_EQ(*pubkey_it, *disk_it);
  }
}

}  // namespace chromeos
