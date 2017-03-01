// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

class PinStorageUnitTest : public testing::Test {
 protected:
  PinStorageUnitTest() : profile_(new TestingProfile()) {}
  ~PinStorageUnitTest() override {}

  // testing::Test:
  void SetUp() override { quick_unlock::EnableForTesting(); }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(PinStorageUnitTest);
};

}  // namespace

// Provides test-only PinStorage APIs.
class PinStorageTestApi {
 public:
  // Does *not* take ownership over |pin_storage|.
  explicit PinStorageTestApi(quick_unlock::PinStorage* pin_storage)
      : pin_storage_(pin_storage) {}

  std::string PinSalt() const { return pin_storage_->PinSalt(); }

  std::string PinSecret() const { return pin_storage_->PinSecret(); }

  bool IsPinAuthenticationAvailable() const {
    return pin_storage_->IsPinAuthenticationAvailable();
  }
  bool TryAuthenticatePin(const std::string& pin) {
    return pin_storage_->TryAuthenticatePin(pin);
  }

 private:
  quick_unlock::PinStorage* pin_storage_;

  DISALLOW_COPY_AND_ASSIGN(PinStorageTestApi);
};

// Verifies that:
// 1. Prefs are initially empty
// 2. Setting a PIN will update the pref system.
// 3. Removing a PIN clears prefs.
TEST_F(PinStorageUnitTest, PinStorageWritesToPrefs) {
  PrefService* prefs = profile_->GetPrefs();

  EXPECT_EQ("", prefs->GetString(prefs::kQuickUnlockPinSalt));
  EXPECT_EQ("", prefs->GetString(prefs::kQuickUnlockPinSecret));

  quick_unlock::PinStorage* pin_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(profile_.get())
          ->pin_storage();
  PinStorageTestApi pin_storage_test(pin_storage);

  pin_storage->SetPin("1111");
  EXPECT_TRUE(pin_storage->IsPinSet());
  EXPECT_EQ(pin_storage_test.PinSalt(),
            prefs->GetString(prefs::kQuickUnlockPinSalt));
  EXPECT_EQ(pin_storage_test.PinSecret(),
            prefs->GetString(prefs::kQuickUnlockPinSecret));
  EXPECT_NE("", pin_storage_test.PinSalt());
  EXPECT_NE("", pin_storage_test.PinSecret());

  pin_storage->RemovePin();
  EXPECT_FALSE(pin_storage->IsPinSet());
  EXPECT_EQ("", prefs->GetString(prefs::kQuickUnlockPinSalt));
  EXPECT_EQ("", prefs->GetString(prefs::kQuickUnlockPinSecret));
}

// Verifies that:
// 1. Initial unlock attempt count is zero.
// 2. Attempting unlock attempts correctly increases unlock attempt count.
// 3. Resetting unlock attempt count correctly sets attempt count to 0.
TEST_F(PinStorageUnitTest, UnlockAttemptCount) {
  quick_unlock::PinStorage* pin_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(profile_.get())
          ->pin_storage();

  EXPECT_EQ(0, pin_storage->unlock_attempt_count());

  pin_storage->AddUnlockAttempt();
  pin_storage->AddUnlockAttempt();
  pin_storage->AddUnlockAttempt();
  EXPECT_EQ(3, pin_storage->unlock_attempt_count());

  pin_storage->ResetUnlockAttemptCount();
  EXPECT_EQ(0, pin_storage->unlock_attempt_count());
}

// Verifies that the correct pin can be used to authenticate.
TEST_F(PinStorageUnitTest, AuthenticationSucceedsWithRightPin) {
  quick_unlock::PinStorage* pin_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(profile_.get())
          ->pin_storage();
  PinStorageTestApi pin_storage_test(pin_storage);

  pin_storage->SetPin("1111");

  EXPECT_TRUE(pin_storage_test.TryAuthenticatePin("1111"));
}

// Verifies that the correct pin will fail to authenticate if too many
// authentication attempts have been made.
TEST_F(PinStorageUnitTest, AuthenticationFailsFromTooManyAttempts) {
  quick_unlock::PinStorage* pin_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(profile_.get())
          ->pin_storage();
  PinStorageTestApi pin_storage_test(pin_storage);

  pin_storage->SetPin("1111");

  // Use up all of the authentication attempts so authentication fails.
  EXPECT_TRUE(pin_storage_test.IsPinAuthenticationAvailable());
  for (int i = 0; i < quick_unlock::PinStorage::kMaximumUnlockAttempts; ++i)
    EXPECT_FALSE(pin_storage_test.TryAuthenticatePin("foobar"));

  // We used up all of the attempts, so entering the right PIN will still fail.
  EXPECT_FALSE(pin_storage_test.IsPinAuthenticationAvailable());
  EXPECT_FALSE(pin_storage_test.TryAuthenticatePin("1111"));
}

}  // namespace chromeos
