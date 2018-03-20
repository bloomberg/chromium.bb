// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the chromeos.quickUnlockPrivate extension API.

#include "chrome/browser/chromeos/extensions/quick_unlock_private/quick_unlock_private_api.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_factory.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_regular.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/fake_extended_authenticator.h"
#include "components/user_manager/scoped_user_manager.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_function_dispatcher.h"

using namespace extensions;
namespace quick_unlock_private = extensions::api::quick_unlock_private;
using CredentialCheck = quick_unlock_private::CredentialCheck;
using CredentialProblem = quick_unlock_private::CredentialProblem;
using CredentialRequirements = quick_unlock_private::CredentialRequirements;
using QuickUnlockMode = quick_unlock_private::QuickUnlockMode;
using QuickUnlockModeList = std::vector<QuickUnlockMode>;
using CredentialList = std::vector<std::string>;

namespace chromeos {
namespace {

constexpr char kTestUserEmail[] = "testuser@gmail.com";
constexpr char kTestUserGaiaId[] = "9876543210";
constexpr char kTestUserEmailHash[] = "testuser@gmail.com-hash";
constexpr char kInvalidToken[] = "invalid";
constexpr char kValidPassword[] = "valid";
constexpr char kInvalidPassword[] = "invalid";

class FakeEasyUnlockService : public EasyUnlockServiceRegular {
 public:
  explicit FakeEasyUnlockService(Profile* profile)
      : EasyUnlockServiceRegular(profile), reauth_count_(0) {}
  ~FakeEasyUnlockService() override {}

  // EasyUnlockServiceRegular:
  void InitializeInternal() override {}
  void ShutdownInternal() override {}
  void HandleUserReauth(const chromeos::UserContext& user_context) override {
    ++reauth_count_;
  }

  void ResetReauthCount() { reauth_count_ = 0; }

  int reauth_count() const { return reauth_count_; }

 private:
  int reauth_count_;

  DISALLOW_COPY_AND_ASSIGN(FakeEasyUnlockService);
};

std::unique_ptr<KeyedService> CreateEasyUnlockServiceForTest(
    content::BrowserContext* context) {
  return std::make_unique<FakeEasyUnlockService>(
      Profile::FromBrowserContext(context));
}

ExtendedAuthenticator* CreateFakeAuthenticator(
    AuthStatusConsumer* auth_status_consumer) {
  const AccountId account_id =
      AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
  UserContext expected_context(account_id);
  expected_context.SetKey(Key(kValidPassword));

  ExtendedAuthenticator* authenticator =
      new FakeExtendedAuthenticator(auth_status_consumer, expected_context);
  return authenticator;
}

void FailIfCalled(const QuickUnlockModeList& modes) {
  FAIL();
}

enum ExpectedPinState {
  PIN_GOOD = 1 << 0,
  PIN_TOO_SHORT = 1 << 1,
  PIN_TOO_LONG = 1 << 2,
  PIN_WEAK_ERROR = 1 << 3,
  PIN_WEAK_WARNING = 1 << 4,
  PIN_CONTAINS_NONDIGIT = 1 << 5
};

}  // namespace

class QuickUnlockPrivateUnitTest : public ExtensionApiUnittest {
 public:
  QuickUnlockPrivateUnitTest()
      : fake_user_manager_(new FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(fake_user_manager_)) {}

 protected:
  void SetUp() override {
    ExtensionApiUnittest::SetUp();

    quick_unlock::EnableForTesting(quick_unlock::PinStorageType::kPrefs);

    // Setup a primary user.
    auto test_account =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
    fake_user_manager_->AddUser(test_account);
    fake_user_manager_->UserLoggedIn(test_account, kTestUserEmailHash, false,
                                     false);

    // Generate an auth token.
    token_ = quick_unlock::QuickUnlockFactory::GetForProfile(profile())
                 ->CreateAuthToken();

    // Ensure that quick unlock is turned off.
    RunSetModes(QuickUnlockModeList{}, CredentialList{});

    modes_changed_handler_ = base::DoNothing();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{EasyUnlockServiceFactory::GetInstance(),
             &CreateEasyUnlockServiceForTest}};
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

  // Wrapper for chrome.quickUnlockPrivate.getAuthToken. Expects the function
  // to succeed and returns the result.
  std::unique_ptr<quick_unlock_private::TokenInfo> GetAuthToken(
      const std::string& password) {
    // Setup a fake authenticator to avoid calling cryptohome methods.
    auto* func = new QuickUnlockPrivateGetAuthTokenFunction();
    func->SetAuthenticatorAllocatorForTesting(
        base::Bind(&CreateFakeAuthenticator));

    auto params = std::make_unique<base::ListValue>();
    params->GetList().push_back(base::Value(password));
    std::unique_ptr<base::Value> result = RunFunction(func, std::move(params));
    EXPECT_TRUE(result);
    auto token_info = quick_unlock_private::TokenInfo::FromValue(*result);
    EXPECT_TRUE(token_info);
    return token_info;
  }

  // Wrapper for chrome.quickUnlockPrivate.getAuthToken with an invalid
  // password. Expects the function to fail and returns the error.
  std::string RunAuthTokenWithInvalidPassword() {
    // Setup a fake authenticator to avoid calling cryptohome methods.
    auto* func = new QuickUnlockPrivateGetAuthTokenFunction();
    func->SetAuthenticatorAllocatorForTesting(
        base::Bind(&CreateFakeAuthenticator));

    auto params = std::make_unique<base::ListValue>();
    params->GetList().push_back(base::Value(kInvalidPassword));
    return RunFunctionAndReturnError(func, std::move(params));
  }

  // Wrapper for chrome.quickUnlockPrivate.setLockScreenEnabled.
  void SetLockScreenEnabled(const std::string& token, bool enabled) {
    auto params = std::make_unique<base::ListValue>();
    params->AppendString(token);
    params->AppendBoolean(enabled);
    RunFunction(new QuickUnlockPrivateSetLockScreenEnabledFunction(),
                std::move(params));
  }

  // Wrapper for chrome.quickUnlockPrivate.setLockScreenEnabled.
  std::string SetLockScreenEnabledWithInvalidToken(bool enabled) {
    auto params = std::make_unique<base::ListValue>();
    params->AppendString(kInvalidToken);
    params->AppendBoolean(enabled);
    return RunFunctionAndReturnError(
        new QuickUnlockPrivateSetLockScreenEnabledFunction(),
        std::move(params));
  }

  // Wrapper for chrome.quickUnlockPrivate.getAvailableModes.
  QuickUnlockModeList GetAvailableModes() {
    // Run the function.
    std::unique_ptr<base::Value> result =
        RunFunction(new QuickUnlockPrivateGetAvailableModesFunction(),
                    std::make_unique<base::ListValue>());

    // Extract the results.
    QuickUnlockModeList modes;

    base::ListValue* list = nullptr;
    EXPECT_TRUE(result->GetAsList(&list));
    for (const base::Value& value : *list) {
      std::string mode;
      EXPECT_TRUE(value.GetAsString(&mode));
      modes.push_back(quick_unlock_private::ParseQuickUnlockMode(mode));
    }

    return modes;
  }

  // Wrapper for chrome.quickUnlockPrivate.getActiveModes.
  QuickUnlockModeList GetActiveModes() {
    std::unique_ptr<base::Value> result =
        RunFunction(new QuickUnlockPrivateGetActiveModesFunction(),
                    std::make_unique<base::ListValue>());

    QuickUnlockModeList modes;

    base::ListValue* list = nullptr;
    EXPECT_TRUE(result->GetAsList(&list));
    for (const base::Value& value : *list) {
      std::string mode;
      EXPECT_TRUE(value.GetAsString(&mode));
      modes.push_back(quick_unlock_private::ParseQuickUnlockMode(mode));
    }

    return modes;
  }

  bool HasFlag(int outcome, int flag) { return (outcome & flag) != 0; }

  // Helper function for checking whether |IsCredentialUsableUsingPin| will
  // return the right message given a pin.
  void CheckPin(int expected_outcome, const std::string& pin) {
    CredentialCheck result = CheckCredentialUsingPin(pin);
    const std::vector<CredentialProblem> errors(result.errors);
    const std::vector<CredentialProblem> warnings(result.warnings);

    // A pin is considered good if it emits no errors or warnings.
    EXPECT_EQ(HasFlag(expected_outcome, PIN_GOOD),
              errors.empty() && warnings.empty());
    EXPECT_EQ(HasFlag(expected_outcome, PIN_TOO_SHORT),
              base::ContainsValue(
                  errors, CredentialProblem::CREDENTIAL_PROBLEM_TOO_SHORT));
    EXPECT_EQ(HasFlag(expected_outcome, PIN_TOO_LONG),
              base::ContainsValue(
                  errors, CredentialProblem::CREDENTIAL_PROBLEM_TOO_LONG));
    EXPECT_EQ(HasFlag(expected_outcome, PIN_WEAK_WARNING),
              base::ContainsValue(
                  warnings, CredentialProblem::CREDENTIAL_PROBLEM_TOO_WEAK));
    EXPECT_EQ(HasFlag(expected_outcome, PIN_WEAK_ERROR),
              base::ContainsValue(
                  errors, CredentialProblem::CREDENTIAL_PROBLEM_TOO_WEAK));
    EXPECT_EQ(
        HasFlag(expected_outcome, PIN_CONTAINS_NONDIGIT),
        base::ContainsValue(
            errors, CredentialProblem::CREDENTIAL_PROBLEM_CONTAINS_NONDIGIT));
  }

  CredentialCheck CheckCredentialUsingPin(const std::string& pin) {
    auto params = std::make_unique<base::ListValue>();
    params->AppendString(ToString(QuickUnlockMode::QUICK_UNLOCK_MODE_PIN));
    params->AppendString(pin);

    std::unique_ptr<base::Value> result = RunFunction(
        new QuickUnlockPrivateCheckCredentialFunction(), std::move(params));

    CredentialCheck function_result;
    EXPECT_TRUE(CredentialCheck::Populate(*result, &function_result));
    return function_result;
  }

  void CheckGetCredentialRequirements(int expected_pin_min_length,
                                      int expected_pin_max_length) {
    auto params = std::make_unique<base::ListValue>();
    params->AppendString(ToString(QuickUnlockMode::QUICK_UNLOCK_MODE_PIN));

    std::unique_ptr<base::Value> result =
        RunFunction(new QuickUnlockPrivateGetCredentialRequirementsFunction(),
                    std::move(params));

    CredentialRequirements function_result;
    EXPECT_TRUE(CredentialRequirements::Populate(*result, &function_result));

    EXPECT_EQ(function_result.min_length, expected_pin_min_length);
    EXPECT_EQ(function_result.max_length, expected_pin_max_length);
  }

  std::unique_ptr<base::ListValue> GetSetModesParams(
      const std::string& token,
      const QuickUnlockModeList& modes,
      const CredentialList& passwords) {
    auto params = std::make_unique<base::ListValue>();
    params->AppendString(token);

    auto serialized_modes = std::make_unique<base::ListValue>();
    for (QuickUnlockMode mode : modes)
      serialized_modes->AppendString(quick_unlock_private::ToString(mode));
    params->Append(std::move(serialized_modes));

    auto serialized_passwords = std::make_unique<base::ListValue>();
    for (const std::string& password : passwords)
      serialized_passwords->AppendString(password);
    params->Append(std::move(serialized_passwords));

    return params;
  }

  // Runs chrome.quickUnlockPrivate.setModes using a valid token. Expects the
  // function to succeed.
  void RunSetModes(const QuickUnlockModeList& modes,
                   const CredentialList& passwords) {
    std::unique_ptr<base::ListValue> params =
        GetSetModesParams(token_, modes, passwords);
    auto* func = new QuickUnlockPrivateSetModesFunction();

    // Stub out event handling since we are not setting up an event router.
    func->SetModesChangedEventHandlerForTesting(modes_changed_handler_);

    // Run the function. Expect a non null result.
    RunFunction(func, std::move(params));

    // Verify that the mode change event handler was run if it was registered.
    // ExpectModesChanged will set expect_modes_changed_ to true and the event
    // handler will set it to false; so if the handler never runs,
    // expect_modes_changed_ will still be true.
    EXPECT_FALSE(expect_modes_changed_) << "Mode change event was not raised";
  }

  // Runs chrome.quickUnlockPrivate.setModes using an invalid token. Expects the
  // function to fail and returns the error.
  std::string RunSetModesWithInvalidToken() {
    std::unique_ptr<base::ListValue> params = GetSetModesParams(
        kInvalidToken, {QuickUnlockMode::QUICK_UNLOCK_MODE_PIN}, {"111111"});
    auto* func = new QuickUnlockPrivateSetModesFunction();

    // Stub out event handling since we are not setting up an event router.
    func->SetModesChangedEventHandlerForTesting(modes_changed_handler_);

    // Run function, expecting it to fail.
    return RunFunctionAndReturnError(func, std::move(params));
  }

  std::string SetModesWithError(const std::string& args) {
    auto* func = new QuickUnlockPrivateSetModesFunction();
    func->SetModesChangedEventHandlerForTesting(base::DoNothing());

    return api_test_utils::RunFunctionAndReturnError(func, args, profile());
  }

  std::string token() { return token_; }

 private:
  // Runs the given |func| with the given |params|.
  std::unique_ptr<base::Value> RunFunction(
      scoped_refptr<UIThreadExtensionFunction> func,
      std::unique_ptr<base::ListValue> params) {
    return api_test_utils::RunFunctionWithDelegateAndReturnSingleResult(
        func, std::move(params), profile(),
        std::make_unique<ExtensionFunctionDispatcher>(profile()),
        api_test_utils::NONE);
  }

  // Runs |func| with |params|. Expects and returns an error result.
  std::string RunFunctionAndReturnError(
      scoped_refptr<UIThreadExtensionFunction> func,
      std::unique_ptr<base::ListValue> params) {
    std::unique_ptr<ExtensionFunctionDispatcher> dispatcher(
        new ExtensionFunctionDispatcher(profile()));
    api_test_utils::RunFunction(func.get(), std::move(params), profile(),
                                std::move(dispatcher), api_test_utils::NONE);
    EXPECT_TRUE(func->GetResultList()->empty());
    return func->GetError();
  }

  // Verifies a mode change event is raised and that |expected| is now the
  // active set of quick unlock modes.
  void ExpectModeList(const QuickUnlockModeList& expected,
                      const QuickUnlockModeList& actual) {
    EXPECT_EQ(expected, actual);
    expect_modes_changed_ = false;
  }

  FakeChromeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
  QuickUnlockPrivateSetModesFunction::ModesChangedEventHandler
      modes_changed_handler_;
  bool expect_modes_changed_ = false;
  std::string token_;

  DISALLOW_COPY_AND_ASSIGN(QuickUnlockPrivateUnitTest);
};

// Verifies that GetAuthTokenValid succeeds when a valid password is provided.
TEST_F(QuickUnlockPrivateUnitTest, GetAuthTokenValid) {
  std::unique_ptr<quick_unlock_private::TokenInfo> token_info =
      GetAuthToken(kValidPassword);

  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(profile());
  EXPECT_EQ(token_info->token, quick_unlock_storage->GetAuthToken());
  EXPECT_EQ(token_info->lifetime_seconds,
            quick_unlock::QuickUnlockStorage::kTokenExpirationSeconds);
}

// Verifies that GetAuthTokenValid fails when an invalid password is provided.
TEST_F(QuickUnlockPrivateUnitTest, GetAuthTokenInvalid) {
  std::string error = RunAuthTokenWithInvalidPassword();
  EXPECT_FALSE(error.empty());
}

// Verifies that setting lock screen enabled modifies the setting.
TEST_F(QuickUnlockPrivateUnitTest, SetLockScreenEnabled) {
  PrefService* pref_service = profile()->GetPrefs();
  bool lock_screen_enabled =
      pref_service->GetBoolean(prefs::kEnableAutoScreenLock);

  SetLockScreenEnabled(token(), !lock_screen_enabled);

  EXPECT_EQ(!lock_screen_enabled,
            pref_service->GetBoolean(prefs::kEnableAutoScreenLock));
}

// Verifies that setting lock screen enabled fails to modify the setting with
// an invalid token.
TEST_F(QuickUnlockPrivateUnitTest, SetLockScreenEnabledFailsWithInvalidToken) {
  PrefService* pref_service = profile()->GetPrefs();
  bool lock_screen_enabled =
      pref_service->GetBoolean(prefs::kEnableAutoScreenLock);

  std::string error =
      SetLockScreenEnabledWithInvalidToken(!lock_screen_enabled);
  EXPECT_FALSE(error.empty());

  EXPECT_EQ(lock_screen_enabled,
            pref_service->GetBoolean(prefs::kEnableAutoScreenLock));
}

// Verifies that this returns PIN for GetAvailableModes.
TEST_F(QuickUnlockPrivateUnitTest, GetAvailableModes) {
  EXPECT_EQ(GetAvailableModes(),
            QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN});
}

// Verifies that SetModes succeeds with a valid token.
TEST_F(QuickUnlockPrivateUnitTest, SetModes) {
  // Verify there is no active mode.
  EXPECT_EQ(GetActiveModes(), QuickUnlockModeList{});

  FakeEasyUnlockService* easy_unlock_service =
      static_cast<FakeEasyUnlockService*>(EasyUnlockService::Get(profile()));
  easy_unlock_service->ResetReauthCount();
  EXPECT_EQ(0, easy_unlock_service->reauth_count());

  RunSetModes(QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN},
              {"111111"});
  EXPECT_EQ(1, easy_unlock_service->reauth_count());
  EXPECT_EQ(GetActiveModes(),
            QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN});
}

// Verifies that an invalid password cannot be used to update the mode list.
TEST_F(QuickUnlockPrivateUnitTest, SetModesFailsWithInvalidPassword) {
  // Verify there is no active mode.
  EXPECT_EQ(GetActiveModes(), QuickUnlockModeList{});

  FakeEasyUnlockService* easy_unlock_service =
      static_cast<FakeEasyUnlockService*>(EasyUnlockService::Get(profile()));
  easy_unlock_service->ResetReauthCount();
  EXPECT_EQ(0, easy_unlock_service->reauth_count());

  // Try to enable PIN, but use an invalid password. Verify that no event is
  // raised and GetActiveModes still returns an empty set.
  FailIfModesChanged();
  std::string error = RunSetModesWithInvalidToken();
  EXPECT_FALSE(error.empty());
  EXPECT_EQ(0, easy_unlock_service->reauth_count());
  EXPECT_EQ(GetActiveModes(), QuickUnlockModeList{});
}

// Verifies that the quickUnlockPrivate.onActiveModesChanged is only raised when
// the active set of modes changes.
TEST_F(QuickUnlockPrivateUnitTest, ModeChangeEventOnlyRaisedWhenModesChange) {
  // Make sure quick unlock is turned off, and then verify that turning it off
  // again does not trigger an event.
  RunSetModes(QuickUnlockModeList{}, CredentialList{});
  FailIfModesChanged();
  RunSetModes(QuickUnlockModeList{}, CredentialList{});

  // Turn on PIN unlock, and then verify turning it on again and also changing
  // the password does not trigger an event.
  ExpectModesChanged(
      QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN});
  RunSetModes(QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN},
              {"111111"});
  FailIfModesChanged();
  RunSetModes(QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN},
              {"222222"});
  RunSetModes(QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN},
              {""});
}

// Ensures that quick unlock can be enabled and disabled by checking the result
// of quickUnlockPrivate.GetActiveModes and PinStorage::IsPinSet.
TEST_F(QuickUnlockPrivateUnitTest, SetModesAndGetActiveModes) {
  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(profile());

  // Update mode to PIN raises an event and updates GetActiveModes.
  ExpectModesChanged(
      QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN});
  RunSetModes(QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN},
              {"111111"});
  EXPECT_EQ(GetActiveModes(),
            QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN});
  EXPECT_TRUE(quick_unlock_storage->pin_storage()->IsPinSet());

  // SetModes can be used to turn off a quick unlock mode.
  ExpectModesChanged(QuickUnlockModeList{});
  RunSetModes(QuickUnlockModeList{}, CredentialList{});
  EXPECT_EQ(GetActiveModes(), QuickUnlockModeList{});
  EXPECT_FALSE(quick_unlock_storage->pin_storage()->IsPinSet());
}

// Verifies that enabling PIN quick unlock actually talks to the PIN subsystem.
TEST_F(QuickUnlockPrivateUnitTest, VerifyAuthenticationAgainstPIN) {
  quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      quick_unlock::QuickUnlockFactory::GetForProfile(profile());

  RunSetModes(QuickUnlockModeList{}, CredentialList{});
  EXPECT_FALSE(quick_unlock_storage->pin_storage()->IsPinSet());

  RunSetModes(QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN},
              {"111111"});
  EXPECT_TRUE(quick_unlock_storage->pin_storage()->IsPinSet());

  quick_unlock_storage->MarkStrongAuth();
  quick_unlock_storage->pin_storage()->ResetUnlockAttemptCount();
  EXPECT_TRUE(quick_unlock_storage->TryAuthenticatePin(
      "111111", Key::KEY_TYPE_PASSWORD_PLAIN));
  EXPECT_FALSE(quick_unlock_storage->TryAuthenticatePin(
      "000000", Key::KEY_TYPE_PASSWORD_PLAIN));
}

// Verifies that the number of modes and the number of passwords given must be
// the same.
TEST_F(QuickUnlockPrivateUnitTest, ThrowErrorOnMismatchedParameterCount) {
  EXPECT_FALSE(SetModesWithError("[\"valid\", [\"PIN\"], []]").empty());
  EXPECT_FALSE(SetModesWithError("[\"valid\", [], [\"11\"]]").empty());
}

// Validates PIN error checking in conjuction with policy-related prefs.
TEST_F(QuickUnlockPrivateUnitTest, CheckCredentialProblemReporting) {
  PrefService* pref_service = profile()->GetPrefs();

  // Verify the pin checks work with the default preferences which are minimum
  // length of 6, maximum length of 0 (no maximum) and no easy to guess check.
  CheckPin(PIN_GOOD, "111112");
  CheckPin(PIN_GOOD, "1111112");
  CheckPin(PIN_GOOD, "1111111111111112");
  CheckPin(PIN_WEAK_WARNING, "111111");
  CheckPin(PIN_TOO_SHORT, "1");
  CheckPin(PIN_TOO_SHORT, "11");
  CheckPin(PIN_TOO_SHORT | PIN_WEAK_WARNING, "111");
  CheckPin(PIN_TOO_SHORT | PIN_CONTAINS_NONDIGIT, "a");
  CheckPin(PIN_CONTAINS_NONDIGIT, "aaaaab");
  CheckPin(PIN_CONTAINS_NONDIGIT | PIN_WEAK_WARNING, "aaaaaa");
  CheckPin(PIN_CONTAINS_NONDIGIT | PIN_WEAK_WARNING, "abcdef");

  // Verify that now if the minimum length is set to 3, PINs of length 3 are
  // accepted.
  pref_service->SetInteger(prefs::kPinUnlockMinimumLength, 3);
  CheckPin(PIN_WEAK_WARNING, "111");

  // Verify setting a nonzero maximum length that is less than the minimum
  // length results in the pin only accepting PINs of length minimum length.
  pref_service->SetInteger(prefs::kPinUnlockMaximumLength, 2);
  pref_service->SetInteger(prefs::kPinUnlockMinimumLength, 4);
  CheckPin(PIN_GOOD, "1112");
  CheckPin(PIN_TOO_SHORT, "112");
  CheckPin(PIN_TOO_LONG, "11112");

  // Verify that now if the maximum length is set to 5, PINs longer than 5 are
  // considered too long and cannot be used.
  pref_service->SetInteger(prefs::kPinUnlockMaximumLength, 5);
  CheckPin(PIN_TOO_LONG | PIN_WEAK_WARNING, "111111");
  CheckPin(PIN_TOO_LONG | PIN_WEAK_WARNING, "1111111");

  // Verify that if both the minimum length and maximum length is set to 4, only
  // 4 digit PINs can be used.
  pref_service->SetInteger(prefs::kPinUnlockMinimumLength, 4);
  pref_service->SetInteger(prefs::kPinUnlockMaximumLength, 4);
  CheckPin(PIN_TOO_SHORT, "122");
  CheckPin(PIN_TOO_LONG, "12222");
  CheckPin(PIN_GOOD, "1222");

  // Set the PINs minimum/maximum lengths back to their defaults.
  pref_service->SetInteger(prefs::kPinUnlockMinimumLength, 4);
  pref_service->SetInteger(prefs::kPinUnlockMaximumLength, 0);

  // Verify that PINs that are weak are flagged as such. See
  // IsPinDifficultEnough in quick_unlock_private_api.cc for the description of
  // a weak pin.
  pref_service->SetBoolean(prefs::kPinUnlockWeakPinsAllowed, false);
  // Good.
  CheckPin(PIN_GOOD, "1112");
  CheckPin(PIN_GOOD, "7890");
  CheckPin(PIN_GOOD, "0987");
  // Same digits.
  CheckPin(PIN_WEAK_ERROR, "1111");
  // Increasing.
  CheckPin(PIN_WEAK_ERROR, "0123");
  CheckPin(PIN_WEAK_ERROR, "3456789");
  // Decreasing.
  CheckPin(PIN_WEAK_ERROR, "3210");
  CheckPin(PIN_WEAK_ERROR, "987654");
  // Too common.
  CheckPin(PIN_WEAK_ERROR, "1212");

  // Verify that if a PIN has more than one error, both are returned.
  CheckPin(PIN_TOO_SHORT | PIN_WEAK_ERROR, "111");
  CheckPin(PIN_TOO_SHORT | PIN_WEAK_ERROR, "234");
}

TEST_F(QuickUnlockPrivateUnitTest, GetCredentialRequirements) {
  PrefService* pref_service = profile()->GetPrefs();

  // Verify that trying out PINs under the minimum/maximum lengths will send the
  // minimum/maximum lengths as additional information for display purposes.
  pref_service->SetInteger(prefs::kPinUnlockMinimumLength, 6);
  pref_service->SetInteger(prefs::kPinUnlockMaximumLength, 8);
  CheckGetCredentialRequirements(6, 8);

  // Verify that by setting a maximum length to be nonzero and smaller than the
  // minimum length, the resulting maxium length will be equal to the minimum
  // length pref.
  pref_service->SetInteger(prefs::kPinUnlockMaximumLength, 4);
  CheckGetCredentialRequirements(6, 6);

  // Verify that the values received from policy are sanitized.
  pref_service->SetInteger(prefs::kPinUnlockMinimumLength, -3);
  pref_service->SetInteger(prefs::kPinUnlockMaximumLength, -3);
  CheckGetCredentialRequirements(1, 0);
}
}  // namespace chromeos
