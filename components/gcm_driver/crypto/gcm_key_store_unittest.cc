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
  void GotKeys(KeyPair* pair_out, std::string* auth_secret_out,
               const KeyPair& pair, const std::string& auth_secret) {
    *pair_out = pair;
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

  KeyPair pair;
  std::string auth_secret;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &pair,
                 &auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(pair.IsInitialized());
  EXPECT_FALSE(pair.has_type());
  EXPECT_EQ(0u, auth_secret.size());

  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.GetKeySuccessRate", 0, 1);  // failure
}

TEST_F(GCMKeyStoreTest, CreateAndGetKeys) {
  KeyPair pair;
  std::string auth_secret;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &pair,
                 &auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(pair.IsInitialized());
  ASSERT_TRUE(pair.has_private_key());
  ASSERT_TRUE(pair.has_public_key());

  EXPECT_GT(pair.public_key().size(), 0u);
  EXPECT_GT(pair.private_key().size(), 0u);

  ASSERT_GT(auth_secret.size(), 0u);

  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.CreateKeySuccessRate", 1, 1);  // success

  KeyPair read_pair;
  std::string read_auth_secret;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &read_pair,
                 &read_auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(read_pair.IsInitialized());

  EXPECT_EQ(pair.type(), read_pair.type());
  EXPECT_EQ(pair.private_key(), read_pair.private_key());
  EXPECT_EQ(pair.public_key(), read_pair.public_key());

  EXPECT_EQ(auth_secret, read_auth_secret);

  histogram_tester()->ExpectBucketCount("GCM.Crypto.GetKeySuccessRate", 1,
                                        1);  // success

  // GetKey should also succeed if fallback_to_empty_authorized_entity is true
  // (fallback should not occur, since an exact match is found).
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      true /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &read_pair,
                 &read_auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(read_pair.IsInitialized());

  EXPECT_EQ(pair.type(), read_pair.type());
  EXPECT_EQ(pair.private_key(), read_pair.private_key());
  EXPECT_EQ(pair.public_key(), read_pair.public_key());

  EXPECT_EQ(auth_secret, read_auth_secret);

  histogram_tester()->ExpectBucketCount("GCM.Crypto.GetKeySuccessRate", 1,
                                        2);  // another success
}

TEST_F(GCMKeyStoreTest, GetKeysFallback) {
  KeyPair pair;
  std::string auth_secret;
  gcm_key_store()->CreateKeys(
      kFakeAppId, "" /* empty authorized entity for non-InstanceID */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &pair,
                 &auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(pair.IsInitialized());
  ASSERT_TRUE(pair.has_private_key());
  ASSERT_TRUE(pair.has_public_key());

  EXPECT_GT(pair.public_key().size(), 0u);
  EXPECT_GT(pair.private_key().size(), 0u);

  ASSERT_GT(auth_secret.size(), 0u);

  histogram_tester()->ExpectBucketCount("GCM.Crypto.CreateKeySuccessRate", 1,
                                        1);  // success

  // GetKeys should fail when fallback_to_empty_authorized_entity is false, as
  // there is not an exact match for kFakeAuthorizedEntity.
  KeyPair read_pair;
  std::string read_auth_secret;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &read_pair,
                 &read_auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(read_pair.IsInitialized());
  EXPECT_FALSE(read_pair.has_type());
  EXPECT_EQ(0u, read_auth_secret.size());

  histogram_tester()->ExpectBucketCount("GCM.Crypto.GetKeySuccessRate", 0,
                                        1);  // failure

  // GetKey should succeed when fallback_to_empty_authorized_entity is true, as
  // falling back to empty authorized entity will match the created key.
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      true /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &read_pair,
                 &read_auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(read_pair.IsInitialized());

  EXPECT_EQ(pair.type(), read_pair.type());
  EXPECT_EQ(pair.private_key(), read_pair.private_key());
  EXPECT_EQ(pair.public_key(), read_pair.public_key());

  EXPECT_EQ(auth_secret, read_auth_secret);

  histogram_tester()->ExpectBucketCount("GCM.Crypto.GetKeySuccessRate", 1,
                                        1);  // success
}

TEST_F(GCMKeyStoreTest, KeysPersistenceBetweenInstances) {
  KeyPair pair;
  std::string auth_secret;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &pair,
                 &auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(pair.IsInitialized());

  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.InitKeyStoreSuccessRate", 1, 1);  // success
  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.LoadKeyStoreSuccessRate", 1, 1);  // success

  // Create a new GCM Key Store instance.
  CreateKeyStore();

  KeyPair read_pair;
  std::string read_auth_secret;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &read_pair,
                 &read_auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(read_pair.IsInitialized());
  EXPECT_TRUE(read_pair.has_type());
  EXPECT_GT(read_auth_secret.size(), 0u);

  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.InitKeyStoreSuccessRate", 1, 2);  // success
  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.LoadKeyStoreSuccessRate", 1, 2);  // success
}

TEST_F(GCMKeyStoreTest, CreateAndRemoveKeys) {
  KeyPair pair;
  std::string auth_secret;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &pair,
                 &auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(pair.IsInitialized());

  KeyPair read_pair;
  std::string read_auth_secret;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &read_pair,
                 &read_auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(read_pair.IsInitialized());
  EXPECT_TRUE(read_pair.has_type());

  gcm_key_store()->RemoveKeys(kFakeAppId, kFakeAuthorizedEntity,
                              base::Bind(&base::DoNothing));

  base::RunLoop().RunUntilIdle();

  histogram_tester()->ExpectBucketCount(
      "GCM.Crypto.RemoveKeySuccessRate", 1, 1);  // success

  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &read_pair,
                 &read_auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(read_pair.IsInitialized());
}

TEST_F(GCMKeyStoreTest, CreateGetAndRemoveKeysSynchronously) {
  KeyPair pair;
  std::string auth_secret;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &pair,
                 &auth_secret));

  // Continue synchronously, without running RunUntilIdle first.
  KeyPair pair_after_create;
  std::string auth_secret_after_create;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                 &pair_after_create, &auth_secret_after_create));

  // Continue synchronously, without running RunUntilIdle first.
  gcm_key_store()->RemoveKeys(kFakeAppId, kFakeAuthorizedEntity,
                              base::Bind(&base::DoNothing));

  // Continue synchronously, without running RunUntilIdle first.
  KeyPair pair_after_remove;
  std::string auth_secret_after_remove;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                 &pair_after_remove, &auth_secret_after_remove));

  base::RunLoop().RunUntilIdle();

  histogram_tester()->ExpectBucketCount("GCM.Crypto.RemoveKeySuccessRate", 1,
                                        1);  // success

  KeyPair pair_after_idle;
  std::string auth_secret_after_idle;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                 &pair_after_idle, &auth_secret_after_idle));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(pair.IsInitialized());
  ASSERT_TRUE(pair_after_create.IsInitialized());
  EXPECT_FALSE(pair_after_remove.IsInitialized());
  EXPECT_FALSE(pair_after_idle.IsInitialized());

  EXPECT_TRUE(pair.has_type());
  EXPECT_EQ(pair.type(), pair_after_create.type());
  EXPECT_EQ(pair.private_key(), pair_after_create.private_key());
  EXPECT_EQ(pair.public_key(), pair_after_create.public_key());

  EXPECT_GT(auth_secret.size(), 0u);
  EXPECT_EQ(auth_secret, auth_secret_after_create);
  EXPECT_EQ("", auth_secret_after_remove);
  EXPECT_EQ("", auth_secret_after_idle);
}

TEST_F(GCMKeyStoreTest, RemoveKeysWildcardAuthorizedEntity) {
  KeyPair pair1, pair2, pair3;
  std::string auth_secret1, auth_secret2, auth_secret3;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &pair1,
                 &auth_secret1));
  gcm_key_store()->CreateKeys(
      kFakeAppId, kSecondFakeAuthorizedEntity,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &pair2,
                 &auth_secret2));
  gcm_key_store()->CreateKeys(
      kSecondFakeAppId, kFakeAuthorizedEntity,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &pair3,
                 &auth_secret3));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(pair1.IsInitialized());
  ASSERT_TRUE(pair2.IsInitialized());
  ASSERT_TRUE(pair3.IsInitialized());

  KeyPair read_pair1, read_pair2, read_pair3;
  std::string read_auth_secret1, read_auth_secret2, read_auth_secret3;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &read_pair1,
                 &read_auth_secret1));
  gcm_key_store()->GetKeys(
      kFakeAppId, kSecondFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &read_pair2,
                 &read_auth_secret2));
  gcm_key_store()->GetKeys(
      kSecondFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &read_pair3,
                 &read_auth_secret3));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(read_pair1.IsInitialized());
  EXPECT_TRUE(read_pair1.has_type());
  ASSERT_TRUE(read_pair2.IsInitialized());
  EXPECT_TRUE(read_pair2.has_type());
  ASSERT_TRUE(read_pair3.IsInitialized());
  EXPECT_TRUE(read_pair3.has_type());

  gcm_key_store()->RemoveKeys(kFakeAppId, "*" /* authorized_entity */,
                              base::Bind(&base::DoNothing));

  base::RunLoop().RunUntilIdle();

  histogram_tester()->ExpectBucketCount("GCM.Crypto.RemoveKeySuccessRate", 1,
                                        1);  // success

  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &read_pair1,
                 &read_auth_secret1));
  gcm_key_store()->GetKeys(
      kFakeAppId, kSecondFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &read_pair2,
                 &read_auth_secret2));
  gcm_key_store()->GetKeys(
      kSecondFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &read_pair3,
                 &read_auth_secret3));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(read_pair1.IsInitialized());
  EXPECT_FALSE(read_pair2.IsInitialized());
  ASSERT_TRUE(read_pair3.IsInitialized());
  EXPECT_TRUE(read_pair3.has_type());
}

TEST_F(GCMKeyStoreTest, GetKeysMultipleAppIds) {
  KeyPair pair;
  std::string auth_secret;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &pair,
                 &auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(pair.IsInitialized());

  gcm_key_store()->CreateKeys(
      kSecondFakeAppId, kSecondFakeAuthorizedEntity,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &pair,
                 &auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(pair.IsInitialized());

  KeyPair read_pair;
  std::string read_auth_secret;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &read_pair,
                 &read_auth_secret));

  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(read_pair.IsInitialized());
  EXPECT_TRUE(read_pair.has_type());
}

TEST_F(GCMKeyStoreTest, SuccessiveCallsBeforeInitialization) {
  KeyPair pair;
  std::string auth_secret;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &pair,
                 &auth_secret));

  // Deliberately do not run the message loop, so that the callback has not
  // been resolved yet. The following EXPECT() ensures this.
  EXPECT_FALSE(pair.IsInitialized());

  KeyPair read_pair;
  std::string read_auth_secret;
  gcm_key_store()->GetKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      false /* fallback_to_empty_authorized_entity */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this), &read_pair,
                 &read_auth_secret));

  EXPECT_FALSE(read_pair.IsInitialized());

  // Now run the message loop. Both tasks should have finished executing. Due
  // to the asynchronous nature of operations, however, we can't rely on the
  // write to have finished before the read begins.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(pair.IsInitialized());
}

TEST_F(GCMKeyStoreTest, CannotShareAppIdFromGCMToInstanceID) {
  KeyPair pair_unused;
  std::string auth_secret_unused;
  gcm_key_store()->CreateKeys(
      kFakeAppId, "" /* empty authorized entity for non-InstanceID */,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                 &pair_unused, &auth_secret_unused));

  base::RunLoop().RunUntilIdle();

  EXPECT_DCHECK_DEATH(
      {
        gcm_key_store()->CreateKeys(
            kFakeAppId, kFakeAuthorizedEntity,
            base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                       &pair_unused, &auth_secret_unused));

        base::RunLoop().RunUntilIdle();
      });
}

TEST_F(GCMKeyStoreTest, CannotShareAppIdFromInstanceIDToGCM) {
  KeyPair pair_unused;
  std::string auth_secret_unused;
  gcm_key_store()->CreateKeys(
      kFakeAppId, kFakeAuthorizedEntity,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                 &pair_unused, &auth_secret_unused));

  base::RunLoop().RunUntilIdle();

  gcm_key_store()->CreateKeys(
      kFakeAppId, kSecondFakeAuthorizedEntity,
      base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                 &pair_unused, &auth_secret_unused));

  base::RunLoop().RunUntilIdle();

  EXPECT_DCHECK_DEATH(
      {
        gcm_key_store()->CreateKeys(
            kFakeAppId, "" /* empty authorized entity for non-InstanceID */,
            base::Bind(&GCMKeyStoreTest::GotKeys, base::Unretained(this),
                       &pair_unused, &auth_secret_unused));

        base::RunLoop().RunUntilIdle();
      });
}

}  // namespace

}  // namespace gcm
