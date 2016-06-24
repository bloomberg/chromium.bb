// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the chromeos.quickUnlockPrivate extension API.

#include "chrome/browser/chromeos/extensions/quick_unlock_private/quick_unlock_private_api.h"

#include "base/bind.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_factory.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chromeos/login/auth/fake_extended_authenticator.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_function_dispatcher.h"

using namespace extensions;
namespace quick_unlock_private = extensions::api::quick_unlock_private;
using QuickUnlockMode = quick_unlock_private::QuickUnlockMode;
using QuickUnlockModeList = std::vector<QuickUnlockMode>;
using CredentialList = std::vector<std::string>;

namespace chromeos {
namespace {

const char* kTestUserEmail = "testuser@gmail.com";
const char* kTestUserEmailHash = "testuser@gmail.com-hash";
const char* kValidPassword = "valid";
const char* kInvalidPassword = "invalid";

chromeos::ExtendedAuthenticator* CreateFakeAuthenticator(
    chromeos::AuthStatusConsumer* auth_status_consumer) {
  AccountId account_id = AccountId::FromUserEmail(kTestUserEmail);
  chromeos::UserContext expected_context(account_id);
  expected_context.SetKey(Key(kValidPassword));

  chromeos::ExtendedAuthenticator* authenticator =
      new chromeos::FakeExtendedAuthenticator(auth_status_consumer,
                                              expected_context);
  return authenticator;
}

void DoNothing(const QuickUnlockModeList& modes) {}

void FailIfCalled(const QuickUnlockModeList& modes) {
  FAIL();
}

}  // namespace

class QuickUnlockPrivateUnitTest : public ExtensionApiUnittest {
 public:
  QuickUnlockPrivateUnitTest()
      : fake_user_manager_(new FakeChromeUserManager()),
        scoped_user_manager_(fake_user_manager_) {}

 protected:
  void SetUp() override {
    ExtensionApiUnittest::SetUp();

    // Setup a primary user.
    auto test_account = AccountId::FromUserEmail(kTestUserEmail);
    fake_user_manager_->AddUser(test_account);
    fake_user_manager_->UserLoggedIn(test_account, kTestUserEmailHash, false);

    // Ensure that quick unlock is turned off.
    SetModes(QuickUnlockModeList{}, CredentialList{});

    modes_changed_handler_ = base::Bind(&DoNothing);
  }

  // If a mode change event is raised, fail the test.
  void FailIfModesChanged() {
    modes_changed_handler_ = base::Bind(&FailIfCalled);
  }

  // If a mode change event is raised, expect the given |modes|.
  void ExpectModesChanged(const QuickUnlockModeList& modes) {
    modes_changed_handler_ =
        base::Bind(&QuickUnlockPrivateUnitTest::ExpectModeList,
                   base::Unretained(this), modes);
    expect_modes_changed_ = true;
  }

  // Wrapper for chrome.quickUnlockPrivate.getAvailableModes.
  QuickUnlockModeList GetAvailableModes() {
    // Run the function.
    std::unique_ptr<base::Value> result =
        RunFunction(new QuickUnlockPrivateGetAvailableModesFunction(),
                    base::WrapUnique(new base::ListValue()));

    // Extract the results.
    QuickUnlockModeList modes;

    base::ListValue* list = nullptr;
    EXPECT_TRUE(result->GetAsList(&list));
    // Consume the unique_ptr by reference so we don't take ownership.
    for (const std::unique_ptr<base::Value>& value : (*list)) {
      std::string mode;
      EXPECT_TRUE(value->GetAsString(&mode));
      modes.push_back(quick_unlock_private::ParseQuickUnlockMode(mode));
    }

    return modes;
  }

  // Wrapper for chrome.quickUnlockPrivate.getActiveModes.
  QuickUnlockModeList GetActiveModes() {
    std::unique_ptr<base::Value> result =
        RunFunction(new QuickUnlockPrivateGetActiveModesFunction(),
                    base::WrapUnique(new base::ListValue()));

    QuickUnlockModeList modes;

    base::ListValue* list = nullptr;
    EXPECT_TRUE(result->GetAsList(&list));
    for (const std::unique_ptr<base::Value>& value : *list) {
      std::string mode;
      EXPECT_TRUE(value->GetAsString(&mode));
      modes.push_back(quick_unlock_private::ParseQuickUnlockMode(mode));
    }

    return modes;
  }

  // Wrapper for chrome.quickUnlockPrivate.setModes that automatically uses a
  // valid password.
  bool SetModes(const QuickUnlockModeList& modes,
                const CredentialList& passwords) {
    return SetModesUsingPassword(kValidPassword, modes, passwords);
  }

  // Wrapper for chrome.quickUnlockPrivate.setModes.
  bool SetModesUsingPassword(const std::string& password,
                             const QuickUnlockModeList& modes,
                             const CredentialList& passwords) {
    // Serialize parameters.
    auto params = base::WrapUnique(new base::ListValue());
    params->AppendString(password);

    auto serialized_modes = base::WrapUnique(new base::ListValue());
    for (QuickUnlockMode mode : modes)
      serialized_modes->AppendString(quick_unlock_private::ToString(mode));
    params->Append(std::move(serialized_modes));

    auto serialized_passwords = base::WrapUnique(new base::ListValue());
    for (const std::string& password : passwords)
      serialized_passwords->AppendString(password);
    params->Append(std::move(serialized_passwords));

    // Setup a fake authenticator so quickUnlockPrivate.checkPassword doesn't
    // call cryptohome methods.
    auto* func = new QuickUnlockPrivateSetModesFunction();
    func->SetAuthenticatorAllocatorForTesting(
        base::Bind(&CreateFakeAuthenticator));

    // Stub out event handling since we are not setting up an event router.
    func->SetModesChangedEventHandlerForTesting(modes_changed_handler_);

    // Run function and extract the result.
    std::unique_ptr<base::Value> func_result =
        RunFunction(func, std::move(params));
    bool result;
    EXPECT_TRUE(func_result->GetAsBoolean(&result));

    // Verify that the mode change event handler was run if it was registered.
    // ExpectModesChanged will set expect_modes_changed_ to true and the event
    // handler will set it to false; so if the handler never runs,
    // expect_modes_changed_ will still be true.
    EXPECT_FALSE(expect_modes_changed_) << "Mode change event was not raised";

    return result;
  }

  std::string SetModesWithError(const std::string& args) {
    auto func = new QuickUnlockPrivateSetModesFunction();
    func->SetAuthenticatorAllocatorForTesting(
        base::Bind(&CreateFakeAuthenticator));
    func->SetModesChangedEventHandlerForTesting(base::Bind(&DoNothing));

    return api_test_utils::RunFunctionAndReturnError(func, args, profile());
  }

  // Returns true if |password| is correct. This calls into SetModes to do so.
  // This will turn off any active quick unlock modes.
  bool CheckPassword(const std::string& password) {
    return SetModesUsingPassword(password, QuickUnlockModeList{},
                                 CredentialList{});
  }

 private:
  // Runs the given |func| with the given |params|.
  std::unique_ptr<base::Value> RunFunction(
      scoped_refptr<UIThreadExtensionFunction> func,
      std::unique_ptr<base::ListValue> params) {
    return std::unique_ptr<base::Value>(
        api_test_utils::RunFunctionWithDelegateAndReturnSingleResult(
            func, std::move(params), profile(),
            base::WrapUnique(new ExtensionFunctionDispatcher(profile())),
            api_test_utils::NONE));
  }

  // Verifies a mode change event is raised and that |expected| is now the
  // active set of quick unlock modes.
  void ExpectModeList(const QuickUnlockModeList& expected,
                      const QuickUnlockModeList& actual) {
    EXPECT_EQ(expected, actual);
    expect_modes_changed_ = false;
  }

  FakeChromeUserManager* fake_user_manager_;
  ScopedUserManagerEnabler scoped_user_manager_;
  QuickUnlockPrivateSetModesFunction::ModesChangedEventHandler
      modes_changed_handler_;
  bool expect_modes_changed_ = false;

  DISALLOW_COPY_AND_ASSIGN(QuickUnlockPrivateUnitTest);
};

// Verify that password checking works.
TEST_F(QuickUnlockPrivateUnitTest, CheckPassword) {
  EXPECT_TRUE(CheckPassword(kValidPassword));
  EXPECT_FALSE(CheckPassword(kInvalidPassword));
}

// Verifies that this returns PIN for GetAvailableModes.
TEST_F(QuickUnlockPrivateUnitTest, GetAvailableModes) {
  EXPECT_EQ(GetAvailableModes(),
            QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN});
}

// Verifies that an invalid password cannot be used to update the mode list.
TEST_F(QuickUnlockPrivateUnitTest, SetModesFailsWithInvalidPassword) {
  // Verify there is no active mode.
  EXPECT_EQ(GetActiveModes(), QuickUnlockModeList{});

  // Try to enable PIN, but use an invalid password. Verify that no event is
  // raised and GetActiveModes still returns an empty set.
  FailIfModesChanged();
  EXPECT_FALSE(SetModesUsingPassword(
      kInvalidPassword,
      QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN}, {"11"}));
  EXPECT_EQ(GetActiveModes(), QuickUnlockModeList{});
}

// Verifies that the quickUnlockPrivate.onActiveModesChanged is only raised when
// the active set of modes changes.
TEST_F(QuickUnlockPrivateUnitTest, ModeChangeEventOnlyRaisedWhenModesChange) {
  // Make sure quick unlock is turned off, and then verify that turning it off
  // again does not trigger an event.
  EXPECT_TRUE(SetModes(QuickUnlockModeList{}, CredentialList{}));
  FailIfModesChanged();
  EXPECT_TRUE(SetModes(QuickUnlockModeList{}, CredentialList{}));

  // Turn on PIN unlock, and then verify turning it on again and also changing
  // the password does not trigger an event.
  ExpectModesChanged(
      QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN});
  EXPECT_TRUE(SetModes(
      QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN}, {"11"}));
  FailIfModesChanged();
  EXPECT_TRUE(SetModes(
      QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN}, {"22"}));
  EXPECT_TRUE(SetModes(
      QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN}, {""}));
}

// Ensures that quick unlock can be enabled and disabled by checking the result
// of quickUnlockPrivate.GetActiveModes and PinStorage::IsPinSet.
TEST_F(QuickUnlockPrivateUnitTest, SetModesAndGetActiveModes) {
  PinStorage* pin_storage = PinStorageFactory::GetForProfile(profile());

  // Update mode to PIN raises an event and updates GetActiveModes.
  ExpectModesChanged(
      QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN});
  EXPECT_TRUE(SetModes(
      QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN}, {"11"}));
  EXPECT_EQ(GetActiveModes(),
            QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN});
  EXPECT_TRUE(pin_storage->IsPinSet());

  // SetModes can be used to turn off a quick unlock mode.
  ExpectModesChanged(QuickUnlockModeList{});
  EXPECT_TRUE(SetModes(QuickUnlockModeList{}, CredentialList{}));
  EXPECT_EQ(GetActiveModes(), QuickUnlockModeList{});
  EXPECT_FALSE(pin_storage->IsPinSet());
}

// Verifies that enabling PIN quick unlock actually talks to the PIN subsystem.
TEST_F(QuickUnlockPrivateUnitTest, VerifyAuthenticationAgainstPIN) {
  PinStorage* pin_storage = PinStorageFactory::GetForProfile(profile());

  EXPECT_TRUE(SetModes(QuickUnlockModeList{}, CredentialList{}));
  EXPECT_FALSE(pin_storage->IsPinSet());

  EXPECT_TRUE(SetModes(
      QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN}, {"11"}));
  EXPECT_TRUE(pin_storage->IsPinSet());

  pin_storage->MarkStrongAuth();
  pin_storage->ResetUnlockAttemptCount();
  EXPECT_TRUE(pin_storage->TryAuthenticatePin("11"));
  EXPECT_FALSE(pin_storage->TryAuthenticatePin("00"));
}

// Verifies that the number of modes and the number of passwords given must be
// the same.
TEST_F(QuickUnlockPrivateUnitTest, ThrowErrorOnMismatchedParameterCount) {
  EXPECT_FALSE(SetModesWithError("[\"valid\", [\"PIN\"], []]").empty());
  EXPECT_FALSE(SetModesWithError("[\"valid\", [], [\"11\"]]").empty());
}

}  // namespace chromeos
