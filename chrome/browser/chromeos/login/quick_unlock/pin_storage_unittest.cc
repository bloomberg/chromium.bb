// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/pin_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void SetConfirmationFrequency(
    PrefService* pref_service,
    chromeos::QuickUnlockPasswordConfirmationFrequency frequency) {
  pref_service->SetInteger(prefs::kQuickUnlockTimeout,
                           static_cast<int>(frequency));
}

class PinStorageUnitTest : public testing::Test {
 protected:
  PinStorageUnitTest() : profile_(new TestingProfile()) {}
  ~PinStorageUnitTest() override {}

  // testing::Test:
  void SetUp() override { chromeos::EnableQuickUnlockForTesting(); }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(PinStorageUnitTest);
};

}  // namespace

// Provides test-only PinStorage APIs.
class PinStorageTestApi {
 public:
  // Does *not* take ownership over |pin_storage|.
  explicit PinStorageTestApi(chromeos::PinStorage* pin_storage)
      : pin_storage_(pin_storage) {}

  // Reduces the amount of strong auth time available by |time_delta|.
  void ReduceRemainingStrongAuthTimeBy(const base::TimeDelta& time_delta) {
    pin_storage_->last_strong_auth_ -= time_delta;
  }

  bool HasStrongAuthInfo() {
    return !pin_storage_->last_strong_auth_.is_null();
  }

  std::string PinSalt() const { return pin_storage_->PinSalt(); }

  std::string PinSecret() const { return pin_storage_->PinSecret(); }

 private:
  chromeos::PinStorage* pin_storage_;

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

  chromeos::PinStorage* pin_storage =
      chromeos::PinStorageFactory::GetForProfile(profile_.get());
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
  chromeos::PinStorage* pin_storage =
      chromeos::PinStorageFactory::GetForProfile(profile_.get());

  EXPECT_EQ(0, pin_storage->unlock_attempt_count());

  pin_storage->AddUnlockAttempt();
  pin_storage->AddUnlockAttempt();
  pin_storage->AddUnlockAttempt();
  EXPECT_EQ(3, pin_storage->unlock_attempt_count());

  pin_storage->ResetUnlockAttemptCount();
  EXPECT_EQ(0, pin_storage->unlock_attempt_count());
}

// Verifies that marking the strong auth makes TimeSinceLastStrongAuth a > zero
// value.
TEST_F(PinStorageUnitTest, TimeSinceLastStrongAuthReturnsPositiveValue) {
  chromeos::PinStorage* pin_storage =
      chromeos::PinStorageFactory::GetForProfile(profile_.get());
  PinStorageTestApi pin_storage_test(pin_storage);

  EXPECT_FALSE(pin_storage_test.HasStrongAuthInfo());

  pin_storage->MarkStrongAuth();

  EXPECT_TRUE(pin_storage_test.HasStrongAuthInfo());
  pin_storage_test.ReduceRemainingStrongAuthTimeBy(
      base::TimeDelta::FromSeconds(60));

  EXPECT_TRUE(pin_storage->TimeSinceLastStrongAuth() >=
              base::TimeDelta::FromSeconds(30));
}

// Verifies that by altering the password confirmation preference, the pin
// storage will request password reconfirmation as expected.
TEST_F(PinStorageUnitTest, QuickUnlockPasswordConfirmationFrequencyPreference) {
  chromeos::PinStorage* pin_storage =
      chromeos::PinStorageFactory::GetForProfile(profile_.get());
  PrefService* pref_service = profile_->GetPrefs();
  PinStorageTestApi test_api(pin_storage);

  // The default is one day, so verify moving the last strong auth time back 13
  // hours should not request strong auth.
  pin_storage->MarkStrongAuth();
  test_api.ReduceRemainingStrongAuthTimeBy(base::TimeDelta::FromHours(13));
  EXPECT_TRUE(pin_storage->HasStrongAuth());

  // Verify moving the last strong auth time back another 13 hours should
  // request strong auth.
  test_api.ReduceRemainingStrongAuthTimeBy(base::TimeDelta::FromHours(13));
  EXPECT_FALSE(pin_storage->HasStrongAuth());

  // Verify that by changing the frequency of required password confirmation to
  // six hours, moving the last strong auth interval back by 4 hours will not
  // trigger a request for strong auth, but moving it by an additional 4 hours
  // will.
  pin_storage->MarkStrongAuth();
  SetConfirmationFrequency(
      pref_service,
      chromeos::QuickUnlockPasswordConfirmationFrequency::SIX_HOURS);
  test_api.ReduceRemainingStrongAuthTimeBy(base::TimeDelta::FromHours(4));
  EXPECT_TRUE(pin_storage->HasStrongAuth());
  test_api.ReduceRemainingStrongAuthTimeBy(base::TimeDelta::FromHours(4));
  EXPECT_FALSE(pin_storage->HasStrongAuth());

  // A valid strong auth becomes invalid if the confirmation frequency is
  // shortened to less than the expiration time.
  pin_storage->MarkStrongAuth();
  SetConfirmationFrequency(
      pref_service,
      chromeos::QuickUnlockPasswordConfirmationFrequency::TWELVE_HOURS);
  EXPECT_TRUE(pin_storage->HasStrongAuth());
  test_api.ReduceRemainingStrongAuthTimeBy(base::TimeDelta::FromHours(8));
  EXPECT_TRUE(pin_storage->HasStrongAuth());
  SetConfirmationFrequency(
      pref_service,
      chromeos::QuickUnlockPasswordConfirmationFrequency::SIX_HOURS);
  EXPECT_FALSE(pin_storage->HasStrongAuth());

  // An expired strong auth becomes usable if the confirmation frequency gets
  // extended past the expiration time.
  pin_storage->MarkStrongAuth();
  SetConfirmationFrequency(
      pref_service,
      chromeos::QuickUnlockPasswordConfirmationFrequency::SIX_HOURS);
  EXPECT_TRUE(pin_storage->HasStrongAuth());
  test_api.ReduceRemainingStrongAuthTimeBy(base::TimeDelta::FromHours(8));
  EXPECT_FALSE(pin_storage->HasStrongAuth());
  SetConfirmationFrequency(
      pref_service,
      chromeos::QuickUnlockPasswordConfirmationFrequency::TWELVE_HOURS);
  EXPECT_TRUE(pin_storage->HasStrongAuth());
}

// Verifies that the correct pin can be used to authenticate.
TEST_F(PinStorageUnitTest, AuthenticationSucceedsWithRightPin) {
  chromeos::PinStorage* pin_storage =
      chromeos::PinStorageFactory::GetForProfile(profile_.get());

  pin_storage->SetPin("1111");

  pin_storage->MarkStrongAuth();
  EXPECT_TRUE(pin_storage->TryAuthenticatePin("1111"));
}

// Verifies that the correct pin will fail to authenticate if too many
// authentication attempts have been made.
TEST_F(PinStorageUnitTest, AuthenticationFailsFromTooManyAttempts) {
  chromeos::PinStorage* pin_storage =
      chromeos::PinStorageFactory::GetForProfile(profile_.get());

  pin_storage->SetPin("1111");

  // Use up all of the authentication attempts so authentication fails.
  pin_storage->MarkStrongAuth();
  EXPECT_TRUE(pin_storage->IsPinAuthenticationAvailable());
  for (int i = 0; i < chromeos::PinStorage::kMaximumUnlockAttempts; ++i)
    EXPECT_FALSE(pin_storage->TryAuthenticatePin("foobar"));

  // We used up all of the attempts, so entering the right PIN will still fail.
  EXPECT_FALSE(pin_storage->IsPinAuthenticationAvailable());
  EXPECT_FALSE(pin_storage->TryAuthenticatePin("1111"));
}

// Verifies that the correct pin will fail to authenticate if it has been too
// long since a strong-auth/password authentication.
TEST_F(PinStorageUnitTest, AuthenticationFailsFromTimeout) {
  chromeos::PinStorage* pin_storage =
      chromeos::PinStorageFactory::GetForProfile(profile_.get());
  PinStorageTestApi pin_storage_test(pin_storage);

  pin_storage->SetPin("1111");
  pin_storage->MarkStrongAuth();
  EXPECT_TRUE(pin_storage->IsPinAuthenticationAvailable());

  // Remove all of the strong auth time so that we have a strong auth timeout.
  pin_storage_test.ReduceRemainingStrongAuthTimeBy(
      base::TimeDelta::FromDays(10));

  EXPECT_FALSE(pin_storage->IsPinAuthenticationAvailable());
}
