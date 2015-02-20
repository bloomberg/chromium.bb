// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_screenlock_state_handler.h"

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/signin/easy_unlock_metrics.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/signin/screenlock_bridge.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Icons used by EasyUnlockScreenlockStateHandler. The icon id values are the
// same as the ones set by ScreenlockBridge.
const char kLockedIconId[] = "locked";
const char kLockedToBeActivatedIconId[] = "locked-to-be-activated";
const char kUnlockedIconId[] = "unlocked";
const char kSpinnerIconId[] = "spinner";
const char kHardlockedIconId[] = "hardlocked";

// Checks if |input| string has any unreplaced placeholders.
bool StringHasPlaceholders(const base::string16& input) {
  std::vector<size_t> offsets;
  std::vector<base::string16> subst;
  subst.push_back(base::string16());

  base::string16 replaced = ReplaceStringPlaceholders(input, subst, &offsets);
  return !offsets.empty();
}

// Fake lock handler to be used in these tests.
class TestLockHandler : public ScreenlockBridge::LockHandler {
 public:
  explicit TestLockHandler(const std::string& user_email)
      : user_email_(user_email),
        show_icon_count_(0u),
        auth_type_(OFFLINE_PASSWORD) {
  }
  ~TestLockHandler() override {}

  // ScreenlockBridge::LockHandler implementation:
  void ShowBannerMessage(const base::string16& message) override {
    ASSERT_FALSE(true) << "Should not be reached.";
  }

  void ShowUserPodCustomIcon(
      const std::string& user_email,
      const ScreenlockBridge::UserPodCustomIconOptions& icon) override {
    ASSERT_EQ(user_email_, user_email);
    ++show_icon_count_;
    last_custom_icon_ = icon.ToDictionaryValue().Pass();
    ValidateCustomIcon();
  }

  void HideUserPodCustomIcon(const std::string& user_email) override {
    ASSERT_EQ(user_email_, user_email);
    last_custom_icon_.reset();
  }

  void EnableInput() override {
    ASSERT_FALSE(true) << "Should not be reached.";
  }

  void SetAuthType(const std::string& user_email,
                   AuthType auth_type,
                   const base::string16& auth_value) override {
    ASSERT_EQ(user_email_, user_email);
    // Generally, this is allowed, but EasyUnlockScreenlockStateHandler should
    // avoid resetting the same auth type.
    EXPECT_NE(auth_type_, auth_type);

    auth_type_ = auth_type;
    auth_value_ = auth_value;
  }

  AuthType GetAuthType(const std::string& user_email) const override {
    EXPECT_EQ(user_email_, user_email);
    return auth_type_;
  }

  ScreenType GetScreenType() const override {
    return LOCK_SCREEN;
  }

  void Unlock(const std::string& user_email) override {
    ASSERT_FALSE(true) << "Should not be reached.";
  }

  void AttemptEasySignin(const std::string& user_email,
                         const std::string& secret,
                         const std::string& key_label) override {
    ASSERT_FALSE(true) << "Should not be reached.";
  }

  // Utility methods used by tests:

  // Gets last set auth value.
  base::string16 GetAuthValue() const {
    return auth_value_;
  }

  // Sets the auth value.
  void SetAuthValue(const base::string16& value) {
    auth_value_ = value;
  }

  // Returns the number of times an icon was shown since the last call to this
  // method.
  size_t GetAndResetShowIconCount() {
    size_t result = show_icon_count_;
    show_icon_count_ = 0u;
    return result;
  }

  // Whether the custom icon is set.
  bool HasCustomIcon() const {
    return last_custom_icon_;
  }

  // If custom icon is set, returns the icon's id.
  // If there is no icon, or if it doesn't have an id set, returns an empty
  // string.
  std::string GetCustomIconId() const {
    std::string result;
    if (last_custom_icon_)
      last_custom_icon_->GetString("id", &result);
    return result;
  }

  // Whether the custom icon is set and it has a tooltip.
  bool CustomIconHasTooltip() const {
    return last_custom_icon_ && last_custom_icon_->HasKey("tooltip");
  }

  // Gets the custom icon's tooltip text, if one is set.
  base::string16 GetCustomIconTooltip() const {
    base::string16 result;
    if (last_custom_icon_)
      last_custom_icon_->GetString("tooltip.text", &result);
    return result;
  }

  // Whether the custom icon's tooltip should be autoshown. If the icon is not
  // set, or it doesn't have a tooltip, returns false.
  bool IsCustomIconTooltipAutoshown() const {
    bool result = false;
    if (last_custom_icon_)
      last_custom_icon_->GetBoolean("tooltip.autoshow", &result);
    return result;
  }

  // Whether the custom icon is set and if has hardlock capability enabed.
  bool CustomIconHardlocksOnClick() const {
    bool result = false;
    if (last_custom_icon_)
      last_custom_icon_->GetBoolean("hardlockOnClick", &result);
    return result;
  }

 private:
  // Does some sanity checks on the last icon set by |ShowUserPodCustomIcon|.
  // It will cause a test failure if the icon is not valid.
  void ValidateCustomIcon() {
    ASSERT_TRUE(last_custom_icon_.get());

    EXPECT_TRUE(last_custom_icon_->HasKey("id"));

    if (last_custom_icon_->HasKey("tooltip")) {
      base::string16 tooltip;
      EXPECT_TRUE(last_custom_icon_->GetString("tooltip.text", &tooltip));
      EXPECT_FALSE(tooltip.empty());
      EXPECT_FALSE(StringHasPlaceholders(tooltip));
    }
  }

  // The fake user email used in test. All methods called on |this| should be
  // associated with this user.
  const std::string user_email_;

  // The last icon set using |SetUserPodCustomIcon|. Call to
  // |HideUserPodcustomIcon| resets it.
  scoped_ptr<base::DictionaryValue> last_custom_icon_;
  size_t show_icon_count_;

  // Auth type and value set using |SetAuthType|.
  AuthType auth_type_;
  base::string16 auth_value_;

  DISALLOW_COPY_AND_ASSIGN(TestLockHandler);
};

class EasyUnlockScreenlockStateHandlerTest : public testing::Test {
 public:
  EasyUnlockScreenlockStateHandlerTest() : user_email_("test_user@gmail.com") {}
  ~EasyUnlockScreenlockStateHandlerTest() override {}

  void SetUp() override {
    // Create and inject fake lock handler to the screenlock bridge.
    lock_handler_.reset(new TestLockHandler(user_email_));
    ScreenlockBridge* screenlock_bridge = ScreenlockBridge::Get();
    screenlock_bridge->SetLockHandler(lock_handler_.get());

    // Create the screenlock state handler object that will be tested.
    state_handler_.reset(new EasyUnlockScreenlockStateHandler(
        user_email_,
        EasyUnlockScreenlockStateHandler::NO_HARDLOCK,
        screenlock_bridge));
  }

  void TearDown() override {
    ScreenlockBridge::Get()->SetLockHandler(NULL);
    lock_handler_.reset();
    state_handler_.reset();
  }

 protected:
  // The state handler that is being tested.
  scoped_ptr<EasyUnlockScreenlockStateHandler> state_handler_;

  // The user associated with |state_handler_|.
  const std::string user_email_;

  // Faked lock handler given to ScreenlockBridge during the test. Abstracts
  // the screen lock UI.
  scoped_ptr<TestLockHandler> lock_handler_;
};

TEST_F(EasyUnlockScreenlockStateHandlerTest, AuthenticatedTrialRun) {
  state_handler_->SetTrialRun();
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::USER_CLICK,
            lock_handler_->GetAuthType(user_email_));

  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kUnlockedIconId, lock_handler_->GetCustomIconId());
  EXPECT_TRUE(lock_handler_->CustomIconHasTooltip());
  EXPECT_TRUE(lock_handler_->IsCustomIconTooltipAutoshown());
  EXPECT_FALSE(lock_handler_->CustomIconHardlocksOnClick());

  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);
  // Duplicated state change should be ignored.
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, AuthenticatedNotInitialRun) {
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::USER_CLICK,
            lock_handler_->GetAuthType(user_email_));

  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kUnlockedIconId, lock_handler_->GetCustomIconId());
  EXPECT_TRUE(lock_handler_->CustomIconHasTooltip());
  EXPECT_FALSE(lock_handler_->IsCustomIconTooltipAutoshown());
  EXPECT_TRUE(lock_handler_->CustomIconHardlocksOnClick());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, IsActive) {
  EXPECT_FALSE(state_handler_->IsActive());
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);
  EXPECT_TRUE(state_handler_->IsActive());
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_INACTIVE);
  EXPECT_FALSE(state_handler_->IsActive());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, BluetoothConnecting) {
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_BLUETOOTH_CONNECTING);
  EXPECT_TRUE(state_handler_->IsActive());

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(user_email_));

  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kSpinnerIconId, lock_handler_->GetCustomIconId());
  EXPECT_FALSE(lock_handler_->CustomIconHasTooltip());
  EXPECT_TRUE(lock_handler_->CustomIconHardlocksOnClick());

  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_BLUETOOTH_CONNECTING);
  // Duplicated state change should be ignored.
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, HardlockedState) {
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::USER_CLICK,
            lock_handler_->GetAuthType(user_email_));

  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::USER_HARDLOCK);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(user_email_));

  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kHardlockedIconId, lock_handler_->GetCustomIconId());
  EXPECT_TRUE(lock_handler_->CustomIconHasTooltip());
  EXPECT_TRUE(lock_handler_->IsCustomIconTooltipAutoshown());
  EXPECT_FALSE(lock_handler_->CustomIconHardlocksOnClick());

  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::USER_HARDLOCK);

  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, HardlockedStateNoPairing) {
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::USER_CLICK,
            lock_handler_->GetAuthType(user_email_));

  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::NO_PAIRING);

  EXPECT_FALSE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(ScreenlockBridge::LockHandler::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(user_email_));
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, StatesWithLockedIcon) {
  std::vector<EasyUnlockScreenlockStateHandler::State> states;
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_NO_BLUETOOTH);
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_NO_PHONE);
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_PHONE_UNSUPPORTED);
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_PHONE_UNLOCKABLE);
  states.push_back(
      EasyUnlockScreenlockStateHandler::STATE_PHONE_NOT_AUTHENTICATED);
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_PHONE_LOCKED);

  for (size_t i = 0; i < states.size(); ++i) {
    state_handler_->ChangeState(states[i]);
    EXPECT_TRUE(state_handler_->IsActive());

    EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount())
        << "State: " << states[i];
    EXPECT_EQ(ScreenlockBridge::LockHandler::OFFLINE_PASSWORD,
              lock_handler_->GetAuthType(user_email_))
        << "State: " << states[i];

    ASSERT_TRUE(lock_handler_->HasCustomIcon())
        << "State: " << states[i];
    EXPECT_EQ(kLockedIconId, lock_handler_->GetCustomIconId())
        << "State: " << states[i];
    EXPECT_TRUE(lock_handler_->CustomIconHasTooltip())
        << "State: " << states[i];
    EXPECT_TRUE(lock_handler_->IsCustomIconTooltipAutoshown())
        << "State: " << states[i];
    EXPECT_TRUE(lock_handler_->CustomIconHardlocksOnClick())
        << "State: " << states[i];

    state_handler_->ChangeState(states[i]);
    EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount())
        << "State: " << states[i];
  }
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, SettingTrialRunUpdatesUI) {
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::USER_CLICK,
            lock_handler_->GetAuthType(user_email_));

  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  ASSERT_FALSE(lock_handler_->IsCustomIconTooltipAutoshown());

  state_handler_->SetTrialRun();

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::USER_CLICK,
            lock_handler_->GetAuthType(user_email_));

  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  ASSERT_TRUE(lock_handler_->IsCustomIconTooltipAutoshown());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest,
       LockScreenClearedOnStateHandlerDestruction) {
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::USER_CLICK,
            lock_handler_->GetAuthType(user_email_));

  ASSERT_TRUE(lock_handler_->HasCustomIcon());

  state_handler_.reset();

  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(user_email_));

  ASSERT_FALSE(lock_handler_->HasCustomIcon());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, StatePreservedWhenScreenUnlocks) {
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::USER_CLICK,
            lock_handler_->GetAuthType(user_email_));
  ASSERT_TRUE(lock_handler_->HasCustomIcon());

  ScreenlockBridge::Get()->SetLockHandler(NULL);
  lock_handler_.reset(new TestLockHandler(user_email_));
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  ScreenlockBridge::Get()->SetLockHandler(lock_handler_.get());

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::USER_CLICK,
            lock_handler_->GetAuthType(user_email_));
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, StateChangeWhileScreenUnlocked) {
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::USER_CLICK,
            lock_handler_->GetAuthType(user_email_));
  ASSERT_TRUE(lock_handler_->HasCustomIcon());

  ScreenlockBridge::Get()->SetLockHandler(NULL);
  lock_handler_.reset(new TestLockHandler(user_email_));
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());

  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_BLUETOOTH_CONNECTING);

  ScreenlockBridge::Get()->SetLockHandler(lock_handler_.get());

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(user_email_));
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kSpinnerIconId, lock_handler_->GetCustomIconId());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest,
       HardlockEnabledAfterInitialUnlock) {
  state_handler_->SetTrialRun();

  std::vector<EasyUnlockScreenlockStateHandler::State> states;
  states.push_back(
      EasyUnlockScreenlockStateHandler::STATE_BLUETOOTH_CONNECTING);
  states.push_back(
      EasyUnlockScreenlockStateHandler::STATE_PHONE_NOT_AUTHENTICATED);
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_NO_BLUETOOTH);
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_NO_PHONE);
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_PHONE_UNSUPPORTED);
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_PHONE_UNLOCKABLE);
  // This one should go last as changing state to AUTHENTICATED enables hard
  // locking.
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);

  for (size_t i = 0; i < states.size(); ++i) {
    state_handler_->ChangeState(states[i]);
    ASSERT_TRUE(lock_handler_->HasCustomIcon()) << "State: " << states[i];
    EXPECT_FALSE(lock_handler_->CustomIconHardlocksOnClick())
        << "State: " << states[i];
  }

  ScreenlockBridge::Get()->SetLockHandler(NULL);
  lock_handler_.reset(new TestLockHandler(user_email_));
  ScreenlockBridge::Get()->SetLockHandler(lock_handler_.get());

  for (size_t i = 0; i < states.size(); ++i) {
    state_handler_->ChangeState(states[i]);
    ASSERT_TRUE(lock_handler_->HasCustomIcon()) << "State: " << states[i];
    EXPECT_TRUE(lock_handler_->CustomIconHardlocksOnClick())
        << "State: " << states[i];
  }
}

TEST_F(EasyUnlockScreenlockStateHandlerTest,
       NoPairingHardlockClearsIcon) {
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_PHONE_LOCKED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kLockedIconId, lock_handler_->GetCustomIconId());

  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::NO_PAIRING);

  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  ASSERT_FALSE(lock_handler_->HasCustomIcon());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, PairingChangedHardlock) {
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_PHONE_LOCKED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kLockedIconId, lock_handler_->GetCustomIconId());

  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::PAIRING_CHANGED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kLockedToBeActivatedIconId, lock_handler_->GetCustomIconId());

  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);

  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kLockedToBeActivatedIconId, lock_handler_->GetCustomIconId());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest,
       PairingChangedHardlockIneffectiveOnInitialRun) {
  state_handler_->SetTrialRun();

  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_PHONE_LOCKED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kLockedIconId, lock_handler_->GetCustomIconId());

  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::PAIRING_CHANGED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kLockedIconId, lock_handler_->GetCustomIconId());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, InactiveStateHidesIcon) {
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);

  ASSERT_TRUE(lock_handler_->HasCustomIcon());

  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_INACTIVE);

  ASSERT_FALSE(lock_handler_->HasCustomIcon());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest,
       AuthenticatedStateClearsPreviousAuthValue) {
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_INACTIVE);

  lock_handler_->SetAuthValue(base::ASCIIToUTF16("xxx"));

  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_EASY_UNLOCK_SCREENLOCK_USER_POD_AUTH_VALUE),
            lock_handler_->GetAuthValue());

  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_NO_PHONE);

  EXPECT_EQ(base::string16(), lock_handler_->GetAuthValue());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest,
       ChangingStateDoesNotAffectAuthValueIfAuthTypeDoesNotChange) {
  lock_handler_->SetAuthValue(base::ASCIIToUTF16("xxx"));

  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_NO_PHONE);
  EXPECT_EQ(base::ASCIIToUTF16("xxx"), lock_handler_->GetAuthValue());

  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_PHONE_NOT_AUTHENTICATED);
  EXPECT_EQ(base::ASCIIToUTF16("xxx"), lock_handler_->GetAuthValue());

  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_BLUETOOTH_CONNECTING);
  EXPECT_EQ(base::ASCIIToUTF16("xxx"), lock_handler_->GetAuthValue());
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kSpinnerIconId, lock_handler_->GetCustomIconId());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, StateChangesIgnoredIfHardlocked) {
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::USER_CLICK,
            lock_handler_->GetAuthType(user_email_));

  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::USER_HARDLOCK);

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(user_email_));
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kHardlockedIconId, lock_handler_->GetCustomIconId());

  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_NO_PHONE);
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());

  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(user_email_));
}

TEST_F(EasyUnlockScreenlockStateHandlerTest,
       LockScreenChangeableOnLockAfterHardlockReset) {
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);

  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::USER_HARDLOCK);
  EXPECT_EQ(2u, lock_handler_->GetAndResetShowIconCount());

  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::NO_HARDLOCK);

  ScreenlockBridge::Get()->SetLockHandler(NULL);
  lock_handler_.reset(new TestLockHandler(user_email_));
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  ScreenlockBridge::Get()->SetLockHandler(lock_handler_.get());

  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_NO_PHONE);

  EXPECT_EQ(2u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_TRUE(lock_handler_->HasCustomIcon());

  ScreenlockBridge::Get()->SetLockHandler(NULL);
  lock_handler_.reset(new TestLockHandler(user_email_));
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  ScreenlockBridge::Get()->SetLockHandler(lock_handler_.get());

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(ScreenlockBridge::LockHandler::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(user_email_));
  EXPECT_EQ(kLockedIconId, lock_handler_->GetCustomIconId());

  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);
  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(ScreenlockBridge::LockHandler::USER_CLICK,
            lock_handler_->GetAuthType(user_email_));
  EXPECT_TRUE(lock_handler_->CustomIconHardlocksOnClick());
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, HardlockStatePersistsOverUnlocks) {
  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);
  state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::USER_HARDLOCK);
  EXPECT_EQ(2u, lock_handler_->GetAndResetShowIconCount());

  ScreenlockBridge::Get()->SetLockHandler(NULL);
  lock_handler_.reset(new TestLockHandler(user_email_));
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  ScreenlockBridge::Get()->SetLockHandler(lock_handler_.get());

  EXPECT_EQ(1u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_EQ(ScreenlockBridge::LockHandler::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(user_email_));
  ASSERT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(kHardlockedIconId, lock_handler_->GetCustomIconId());

  state_handler_->ChangeState(
      EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);
  EXPECT_EQ(0u, lock_handler_->GetAndResetShowIconCount());
  EXPECT_TRUE(lock_handler_->HasCustomIcon());
  EXPECT_EQ(ScreenlockBridge::LockHandler::OFFLINE_PASSWORD,
            lock_handler_->GetAuthType(user_email_));
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, NoOverrideOnlineSignin) {
  lock_handler_->SetAuthType(user_email_,
                             ScreenlockBridge::LockHandler::ONLINE_SIGN_IN,
                             base::string16());

  std::vector<EasyUnlockScreenlockStateHandler::State> states;
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_NO_BLUETOOTH);
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_NO_PHONE);
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_PHONE_UNSUPPORTED);
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_PHONE_UNLOCKABLE);
  states.push_back(
      EasyUnlockScreenlockStateHandler::STATE_PHONE_NOT_AUTHENTICATED);
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_PHONE_LOCKED);
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_PHONE_UNLOCKABLE);
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_PHONE_UNSUPPORTED);
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_RSSI_TOO_LOW);
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_TX_POWER_TOO_HIGH);
  states.push_back(EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED);

  for (size_t i = 0; i < states.size(); ++i) {
    state_handler_->ChangeState(states[i]);
    EXPECT_EQ(ScreenlockBridge::LockHandler::ONLINE_SIGN_IN,
              lock_handler_->GetAuthType(user_email_));
    EXPECT_FALSE(lock_handler_->HasCustomIcon());
  }

  std::vector<EasyUnlockScreenlockStateHandler::HardlockState> hardlock_states;
  hardlock_states.push_back(EasyUnlockScreenlockStateHandler::NO_HARDLOCK);
  hardlock_states.push_back(EasyUnlockScreenlockStateHandler::USER_HARDLOCK);
  hardlock_states.push_back(EasyUnlockScreenlockStateHandler::PAIRING_CHANGED);
  hardlock_states.push_back(EasyUnlockScreenlockStateHandler::PAIRING_ADDED);
  hardlock_states.push_back(EasyUnlockScreenlockStateHandler::NO_PAIRING);
  hardlock_states.push_back(EasyUnlockScreenlockStateHandler::LOGIN_FAILED);

  for (size_t i = 0; i < hardlock_states.size(); ++i) {
    state_handler_->SetHardlockState(hardlock_states[i]);
    EXPECT_EQ(ScreenlockBridge::LockHandler::ONLINE_SIGN_IN,
              lock_handler_->GetAuthType(user_email_));
    EXPECT_FALSE(lock_handler_->HasCustomIcon());
  }
}

TEST_F(EasyUnlockScreenlockStateHandlerTest, TrialRunMetrics) {
  base::HistogramTester histogram_tester;

  // Simulate the user clicking on the lock icon twice outside of a trial run.
  // No trial run metrics should be recorded.
  state_handler_->RecordClickOnLockIcon();
  state_handler_->RecordClickOnLockIcon();
  histogram_tester.ExpectTotalCount("EasyUnlock.TrialRun.Events", 0);

  // Simulate the user clicking on the lock icon three times during a trial run.
  state_handler_->SetTrialRun();
  state_handler_->RecordClickOnLockIcon();
  state_handler_->RecordClickOnLockIcon();
  state_handler_->RecordClickOnLockIcon();
  histogram_tester.ExpectTotalCount("EasyUnlock.TrialRun.Events", 4);
  histogram_tester.ExpectBucketCount("EasyUnlock.TrialRun.Events",
                                     EASY_UNLOCK_TRIAL_RUN_EVENT_LAUNCHED, 1);
  histogram_tester.ExpectBucketCount(
      "EasyUnlock.TrialRun.Events",
      EASY_UNLOCK_TRIAL_RUN_EVENT_CLICKED_LOCK_ICON, 3);
}

}  // namespace
