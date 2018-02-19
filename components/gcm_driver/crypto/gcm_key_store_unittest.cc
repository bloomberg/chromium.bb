// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_key_store.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

using ECPrivateKeyUniquePtr = std::unique_ptr<crypto::ECPrivateKey>;

const char kFakeAppId[] = "my_app_id";
const char kSecondFakeAppId[] = "my_other_app_id";
const char kFakeAuthorizedEntity[] = "my_sender_id";
const char kSecondFakeAuthorizedEntity[] = "my_other_sender_id";

class GCMKeyStoreTest : public ::testing::Test {
 public:
  GCMKeyStoreTest() {}
  ~GCMKeyStoreTest() override {}

  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    CreateKeyStore();
  }

  void TearDown() override {
    gcm_key_store_.reset();

    // |gcm_key_store_| owns a ProtoDatabaseImpl whose destructor deletes the
    // underlying LevelDB database on the task runner.
    base::RunLoop().RunUntilIdle();
  }

  // Creates the GCM Key Store instance. May be called from within a test's body
  // to re-create the key store, causing the database to re-open.
  void CreateKeyStore() {
    gcm_key_store_.reset(new GCMKeyStore(scoped_temp_dir_.GetPath(),
                                         message_loop_.task_runner()));
  }

  // Callback to use with GCMKeyStore::{GetKeys, CreateKeys} calls.
  void GotKeys(ECPrivateKeyUniquePtr* key_out,
               std::string* auth_secret_out,
               ECPrivateKeyUniquePtr key,
               const std::string& auth_secret) {
    *key_out = std::move(key);
    *auth_secret_out = auth_secret;
  }

 protected:
  GCMKeyStore* gcm_key_store() { return gcm_key_store_.get(); }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  base::MessageLoop message_loop_;
  base::ScopedTempDir scoped_temp_dir_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<GCMKeyStore> gcm_key_store_;
};

TEST_F(GCMKeyStoreTest, EmptyByDefault) {
  // The key store is initialized lazily, so this histogram confirms that
  // calling the constructor does not in fact cause initialization.
  histogram_tester()->ExpectTotalCount(
      "GCM.Crypto.InitKeyStoreSuccessRate", 0);

  ECPrivateKeyUniquePtr key;
  std::string auth_secret;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                     &auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(key);
  EXPECT_EQ(0u, auth_secret.size());

  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.GetKeySuccessRate", 0, 1);  // failure
}

TEST_F(GCMKeyStoreTest, CreateAndGetKeys) {
  ECPrivateKeyUniquePtr key;
  std::string auth_secret;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                     &auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(key);
  std::string public_key, private_key;
  ASSERT_TRUE(GetRawPrivateKey(*key, &private_key));
  ASSERT_TRUE(GetRawPublicKey(*key, &public_key));

  EXPECT_GT(public_key.size(), 0u);
  EXPECT_GT(private_key.size(), 0u);

  ASSERT_GT(auth_secret.size(), 0u);
  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.CreateKeySuccessRate", 1, 1);  // success

  ECPrivateKeyUniquePtr read_key;
  std::string read_auth_secret;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key, &read_auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(read_key);
  std::string read_public_key, read_private_key;
  ASSERT_TRUE(GetRawPrivateKey(*read_key, &read_private_key));
  ASSERT_TRUE(GetRawPublicKey(*read_key, &read_public_key));
  ASSERT_EQ(read_private_key, private_key);
  ASSERT_EQ(read_public_key, public_key);
  EXPECT_EQ(auth_secret, read_auth_secret);

  histogram_tester()->ExpectBucketCount("GCM.Crypto.GetKeySuccessRate", 1,
                                        1);  // success

  // GetKey should also succeed if fallback_to_empty_authorized_entity is true
  // (fallback should not occur, since an exact match is found).
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      true /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key, &read_auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(read_key);

  ASSERT_TRUE(GetRawPrivateKey(*read_key, &read_private_key));
  ASSERT_TRUE(GetRawPublicKey(*read_key, &read_public_key));
  ASSERT_EQ(read_private_key, private_key);
  ASSERT_EQ(read_public_key, public_key);
  EXPECT_EQ(auth_secret, read_auth_secret);

  histogram_tester()->ExpectBucketCount("GCM.Crypto.GetKeySuccessRate", 1,
                                        2);  // another success
}

TEST_F(GCMKeyStoreTest, GetKeysFallback) {
  ECPrivateKeyUniquePtr key;
  std::string auth_secret;
  gcm_key_store()->CreateKeys(
      kFakeAppId, "" /* empty authorized entity for non-InstanceID */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                     &auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(key);

  std::string public_key, private_key;
  ASSERT_TRUE(GetRawPrivateKey(*key, &private_key));
  ASSERT_TRUE(GetRawPublicKey(*key, &public_key));

  EXPECT_GT(public_key.size(), 0u);
  EXPECT_GT(private_key.size(), 0u);
  ASSERT_GT(auth_secret.size(), 0u);

  histogram_tester()->ExpectBucketCount("GCM.Crypto.CreateKeySuccessRate", 1,
                                        1);  // success

  // GetKeys should fail when fallback_to_empty_authorized_entity is false, as
  // there is not an exact match for kFakeAuthorizedEntity.
  ECPrivateKeyUniquePtr read_key;
  std::string read_auth_secret;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key, &read_auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(read_key);
  EXPECT_EQ(0u, read_auth_secret.size());

  histogram_tester()->ExpectBucketCount("GCM.Crypto.GetKeySuccessRate", 0,
                                        1);  // failure

  // GetKey should succeed when fallback_to_empty_authorized_entity is true, as
  // falling back to empty authorized entity will match the created key.
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      true /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key, &read_auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(read_key);

  std::string read_public_key, read_private_key;
  ASSERT_TRUE(GetRawPrivateKey(*key, &read_private_key));
  ASSERT_TRUE(GetRawPublicKey(*key, &read_public_key));
  EXPECT_EQ(private_key, read_private_key);
  EXPECT_EQ(public_key, read_public_key);

  EXPECT_EQ(auth_secret, read_auth_secret);

  histogram_tester()->ExpectBucketCount("GCM.Crypto.GetKeySuccessRate", 1,
                                        1);  // success
}

TEST_F(GCMKeyStoreTest, KeysPersistenceBetweenInstances) {
  ECPrivateKeyUniquePtr key;
  std::string auth_secret;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                     &auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(key);

  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.InitKeyStoreSuccessRate", 1, 1);  // success
  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.LoadKeyStoreSuccessRate", 1, 1);  // success

  // Create a new GCM Key Store instance.
  CreateKeyStore();

  ECPrivateKeyUniquePtr read_key;
  std::string read_auth_secret;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key, &read_auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(read_key);
  EXPECT_GT(read_auth_secret.size(), 0u);

  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.InitKeyStoreSuccessRate", 1, 2);  // success
  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.LoadKeyStoreSuccessRate", 1, 2);  // success
}

TEST_F(GCMKeyStoreTest, CreateAndRemoveKeys) {
  ECPrivateKeyUniquePtr key;
  std::string auth_secret;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                     &auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(key);

  ECPrivateKeyUniquePtr read_key;
  std::string read_auth_secret;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key, &read_auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(read_key);

  gcm_key_store()->RemoveKeys(kFakeAppId, kFakeAuthorizedEntity,
                              base::Bind(&base::DoNothing));

  base::RunLoop().RunUntilIdle();

  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.RemoveKeySuccessRate", 1, 1);  // success

  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key, &read_auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(read_key);
}

TEST_F(GCMKeyStoreTest, CreateGetAndRemoveKeysSynchronously) {
  ECPrivateKeyUniquePtr key;
  std::string auth_secret;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                     &auth_secret));

  // Continue synchronously, without running RunUntilIdle first.
  ECPrivateKeyUniquePtr key_after_create;
  std::string auth_secret_after_create;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &key_after_create, &auth_secret_after_create));

  // Continue synchronously, without running RunUntilIdle first.
  gcm_key_store()->RemoveKeys(kFakeAppId, kFakeAuthorizedEntity,
                              base::Bind(&base::DoNothing));

  // Continue synchronously, without running RunUntilIdle first.
  ECPrivateKeyUniquePtr key_after_remove;
  std::string auth_secret_after_remove;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &key_after_remove, &auth_secret_after_remove));

  base::RunLoop().RunUntilIdle();

  histogram_tester()->ExpectBucketCount("GCM.Crypto.RemoveKeySuccessRate", 1,
                                        1);  // success

  ECPrivateKeyUniquePtr key_after_idle;
  std::string auth_secret_after_idle;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &key_after_idle, &auth_secret_after_idle));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(key);
  ASSERT_TRUE(key_after_create);
  EXPECT_FALSE(key_after_remove);
  EXPECT_FALSE(key_after_idle);

  std::string public_key, public_key_after_create;
  ASSERT_TRUE(GetRawPublicKey(*key, &public_key));
  ASSERT_TRUE(GetRawPublicKey(*key, &public_key_after_create));
  EXPECT_EQ(public_key, public_key_after_create);

  EXPECT_GT(auth_secret.size(), 0u);
  EXPECT_EQ(auth_secret, auth_secret_after_create);
  EXPECT_EQ("", auth_secret_after_remove);
  EXPECT_EQ("", auth_secret_after_idle);
}

TEST_F(GCMKeyStoreTest, RemoveKeysWildcardAuthorizedEntity) {
  ECPrivateKeyUniquePtr key1, key2, key3;
  std::string auth_secret1, auth_secret2, auth_secret3;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key1,
                     &auth_secret1));
  gcm_key_store()->CreateKeys(
      kFakeAppId, kSecondFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key2,
                     &auth_secret2));
  gcm_key_store()->CreateKeys(
      kSecondFakeAppId, kFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key3,
                     &auth_secret3));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(key1);
  ASSERT_TRUE(key2);
  ASSERT_TRUE(key3);

  ECPrivateKeyUniquePtr read_key1, read_key2, read_key3;
  std::string read_auth_secret1, read_auth_secret2, read_auth_secret3;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key1, &read_auth_secret1));
  gcm_key_store()->GetKeys(
      kFakeAppId, kSecondFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key2, &read_auth_secret2));
  gcm_key_store()->GetKeys(
      kSecondFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key3, &read_auth_secret3));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(read_key1);
  ASSERT_TRUE(read_key2);
  ASSERT_TRUE(read_key3);

  gcm_key_store()->RemoveKeys(kFakeAppId, "*" /* authorized_entity */,
                              base::Bind(&base::DoNothing));

  base::RunLoop().RunUntilIdle();

  histogram_tester()->ExpectBucketCount("GCM.Crypto.RemoveKeySuccessRate", 1,
                                        1);  // success

  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key1, &read_auth_secret1));
  gcm_key_store()->GetKeys(
      kFakeAppId, kSecondFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key2, &read_auth_secret2));
  gcm_key_store()->GetKeys(
      kSecondFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key3, &read_auth_secret3));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(read_key1);
  EXPECT_FALSE(read_key2);
  ASSERT_TRUE(read_key3);
}

TEST_F(GCMKeyStoreTest, GetKeysMultipleAppIds) {
  ECPrivateKeyUniquePtr key;
  std::string auth_secret;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                     &auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(key);

  gcm_key_store()->CreateKeys(
      kSecondFakeAppId, kSecondFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                     &auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(key);

  ECPrivateKeyUniquePtr read_key;
  std::string read_auth_secret;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key, &read_auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(read_key);
}

TEST_F(GCMKeyStoreTest, SuccessiveCallsBeforeInitialization) {
  ECPrivateKeyUniquePtr key;
  std::string auth_secret;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &key,
                     &auth_secret));

  // Deliberately do not run the message loop, so that the callback has not
  // been resolved yet. The following EXPECT() ensures this.
  EXPECT_FALSE(key);

  ECPrivateKeyUniquePtr read_key;
  std::string read_auth_secret;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &read_key, &read_auth_secret));

  EXPECT_FALSE(read_key);

  // Now run the message loop. Both tasks should have finished executing. Due
  // to the asynchronous nature of operations, however, we can't rely on the
  // write to have finished before the read begins.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(key);
}

TEST_F(GCMKeyStoreTest, CannotShareAppIdFromGCMToInstanceID) {
  ECPrivateKeyUniquePtr key_unused;
  std::string auth_secret_unused;
  gcm_key_store()->CreateKeys(
      kFakeAppId, "" /* empty authorized entity for non-InstanceID */,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &key_unused, &auth_secret_unused));

  base::RunLoop().RunUntilIdle();

  EXPECT_DCHECK_DEATH({
    gcm_key_store()->CreateKeys(
        kFakeAppId, kFakeAuthorizedEntity,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                       &key_unused, &auth_secret_unused));

    base::RunLoop().RunUntilIdle();
  });
}

TEST_F(GCMKeyStoreTest, CannotShareAppIdFromInstanceIDToGCM) {
  ECPrivateKeyUniquePtr key_unused;
  std::string auth_secret_unused;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &key_unused, &auth_secret_unused));

  base::RunLoop().RunUntilIdle();

  gcm_key_store()->CreateKeys(
      kFakeAppId, kSecondFakeAuthorizedEntity,
      base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                     &key_unused, &auth_secret_unused));

  base::RunLoop().RunUntilIdle();

  EXPECT_DCHECK_DEATH({
    gcm_key_store()->CreateKeys(
        kFakeAppId, "" /* empty authorized entity for non-InstanceID */,
        base::BindOnce(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                       &key_unused, &auth_secret_unused));

    base::RunLoop().RunUntilIdle();
  });
}

}  // namespace

}  // namespace gcm
