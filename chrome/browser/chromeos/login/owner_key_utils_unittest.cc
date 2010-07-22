// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/owner_key_utils.h"

#include <cert.h>
#include <keyhi.h>
#include <keythi.h>  // KeyType enum
#include <pk11pub.h>
#include <stdlib.h>

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/nss_util_internal.h"
#include "base/nss_util.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class OwnerKeyUtilsTest : public ::testing::Test {
 public:
  OwnerKeyUtilsTest()
      : private_key_(NULL),
        public_key_(NULL),
        utils_(OwnerKeyUtils::Create()) {

  }
  virtual ~OwnerKeyUtilsTest() {}

  virtual void SetUp() {
    base::OpenPersistentNSSDB();
  }

  virtual void TearDown() {
    if (private_key_) {
      PK11_DestroyTokenObject(private_key_->pkcs11Slot, private_key_->pkcs11ID);
      SECKEY_DestroyPrivateKey(private_key_);
    }
    if (public_key_) {
      PK11_DestroyTokenObject(public_key_->pkcs11Slot, public_key_->pkcs11ID);
      SECKEY_DestroyPublicKey(public_key_);
    }
  }

  SECKEYPrivateKey* private_key_;
  SECKEYPublicKey* public_key_;
  scoped_ptr<OwnerKeyUtils> utils_;
};

TEST_F(OwnerKeyUtilsTest, KeyGenerate) {
  EXPECT_TRUE(utils_->GenerateKeyPair(&private_key_, &public_key_));
  EXPECT_TRUE(private_key_ != NULL);
  ASSERT_TRUE(public_key_ != NULL);
  EXPECT_EQ(public_key_->keyType, rsaKey);
}

TEST_F(OwnerKeyUtilsTest, ExportImportPublicKey) {
  EXPECT_TRUE(utils_->GenerateKeyPair(&private_key_, &public_key_));

  ScopedTempDir tmpdir;
  FilePath tmpfile;
  ASSERT_TRUE(tmpdir.CreateUniqueTempDir());
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(tmpdir.path(), &tmpfile));

  EXPECT_TRUE(utils_->ExportPublicKey(public_key_, tmpfile));

  // Now, verify that we can look up the private key, given the public key
  // we exported.  We'll create
  // an ID from the key, and then use that ID to query the token in the
  // default slot for a matching private key.  Then we'll make sure it's
  // the same as |private_key_|
  PK11SlotInfo* slot = NULL;
  SECItem* ck_id = NULL;
  SECKEYPublicKey* from_disk = NULL;
  SECKEYPrivateKey* found = NULL;

  slot = base::GetDefaultNSSKeySlot();
  EXPECT_TRUE(slot != NULL);
  if (NULL == slot)
    goto cleanup;

  from_disk = utils_->ImportPublicKey(tmpfile);
  ASSERT_TRUE(from_disk != NULL);

  ck_id = PK11_MakeIDFromPubKey(&(from_disk->u.rsa.modulus));
  EXPECT_TRUE(ck_id != NULL);
  if (NULL == ck_id)
    goto cleanup;

  found = PK11_FindKeyByKeyID(slot, ck_id, NULL);
  EXPECT_TRUE(found != NULL);
  if (NULL == found)
    goto cleanup;

  EXPECT_EQ(private_key_->pkcs11ID, found->pkcs11ID);

 cleanup:
  if (slot)
    PK11_FreeSlot(slot);
  if (from_disk)
    SECKEY_DestroyPublicKey(from_disk);
  if (found)
    SECKEY_DestroyPrivateKey(found);
  if (ck_id)
    SECITEM_ZfreeItem(ck_id, PR_TRUE);
}

}  // namespace chromeos
