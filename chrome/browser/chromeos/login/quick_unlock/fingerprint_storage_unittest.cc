// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

class FingerprintStorageUnitTest : public testing::Test {
 protected:
  FingerprintStorageUnitTest() : profile_(new TestingProfile()) {}
  ~FingerprintStorageUnitTest() override {}

  // testing::Test:
  void SetUp() override { quick_unlock::EnableForTesting(); }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(FingerprintStorageUnitTest);
};

}  // namespace

// Provides test-only FingerprintStorage APIs.
class FingerprintStorageTestApi {
 public:
  // Does *not* take ownership over |fingerprint_storage|.
  explicit FingerprintStorageTestApi(
      quick_unlock::FingerprintStorage* fingerprint_storage)
      : fingerprint_storage_(fingerprint_storage) {}

  void SetEnrollments(bool has_enrollments) {
    fingerprint_storage_->has_enrollments_ = has_enrollments;
  }

  bool IsFingerprintAuthenticationAvailable() const {
    return fingerprint_storage_->IsFingerprintAuthenticationAvailable();
  }

 private:
  quick_unlock::FingerprintStorage* fingerprint_storage_;

  DISALLOW_COPY_AND_ASSIGN(FingerprintStorageTestApi);
};

// Verifies that:
// 1. Initial unlock attempt count is zero.
// 2. Attempting unlock attempts correctly increases unlock attempt count.
// 3. Resetting unlock attempt count correctly sets attempt count to 0.
TEST_F(FingerprintStorageUnitTest, UnlockAttemptCount) {
  quick_unlock::FingerprintStorage* fingerprint_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(profile_.get())
          ->fingerprint_storage();

  EXPECT_EQ(0, fingerprint_storage->unlock_attempt_count());

  fingerprint_storage->AddUnlockAttempt();
  fingerprint_storage->AddUnlockAttempt();
  fingerprint_storage->AddUnlockAttempt();
  EXPECT_EQ(3, fingerprint_storage->unlock_attempt_count());

  fingerprint_storage->ResetUnlockAttemptCount();
  EXPECT_EQ(0, fingerprint_storage->unlock_attempt_count());
}

// Verifies that authentication is not available when
// 1. No enrollments registered
// 2. Too many authentication attempts
TEST_F(FingerprintStorageUnitTest, AuthenticationUnAvailable) {
  quick_unlock::FingerprintStorage* fingerprint_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(profile_.get())
          ->fingerprint_storage();
  FingerprintStorageTestApi test_api(fingerprint_storage);

  EXPECT_FALSE(fingerprint_storage->HasEnrollment());
  test_api.SetEnrollments(true);
  EXPECT_TRUE(fingerprint_storage->HasEnrollment());
  EXPECT_EQ(0, fingerprint_storage->unlock_attempt_count());
  EXPECT_TRUE(test_api.IsFingerprintAuthenticationAvailable());

  // No enrollment registered makes fingerprint authentication unavailable.
  test_api.SetEnrollments(false);
  EXPECT_FALSE(test_api.IsFingerprintAuthenticationAvailable());
  test_api.SetEnrollments(true);
  EXPECT_TRUE(test_api.IsFingerprintAuthenticationAvailable());

  // Too many authentication attempts make fingerprint authentication
  // unavailable.
  for (int i = 0; i < quick_unlock::FingerprintStorage::kMaximumUnlockAttempts;
       ++i) {
    fingerprint_storage->AddUnlockAttempt();
  }
  EXPECT_FALSE(test_api.IsFingerprintAuthenticationAvailable());
  fingerprint_storage->ResetUnlockAttemptCount();
  EXPECT_TRUE(test_api.IsFingerprintAuthenticationAvailable());
}

}  // namespace chromeos
