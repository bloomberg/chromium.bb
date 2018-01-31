// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/quick_unlock_private/quick_unlock_private_api.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/stl_util.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/extended_authenticator.h"
#include "chromeos/login/auth/user_context.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/event_router.h"

namespace extensions {

namespace quick_unlock_private = api::quick_unlock_private;
namespace SetModes = quick_unlock_private::SetModes;
namespace GetActiveModes = quick_unlock_private::GetActiveModes;
namespace CheckCredential = quick_unlock_private::CheckCredential;
namespace GetCredentialRequirements =
    quick_unlock_private::GetCredentialRequirements;
namespace GetAvailableModes = quick_unlock_private::GetAvailableModes;
namespace OnActiveModesChanged = quick_unlock_private::OnActiveModesChanged;
using CredentialProblem = quick_unlock_private::CredentialProblem;
using CredentialCheck = quick_unlock_private::CredentialCheck;
using CredentialRequirements = quick_unlock_private::CredentialRequirements;
using QuickUnlockMode = quick_unlock_private::QuickUnlockMode;
using QuickUnlockModeList = std::vector<QuickUnlockMode>;

namespace {

const char kModesAndCredentialsLengthMismatch[] =
    "|modes| and |credentials| must have the same number of elements";
const char kMultipleModesNotSupported[] =
    "At most one quick unlock mode can be active.";

// PINs greater in length than |kMinLengthForWeakPin| will be checked for
// weakness.
constexpr size_t kMinLengthForNonWeakPin = 2U;

// A list of the most commmonly used PINs, whose digits are not all the same,
// increasing or decreasing. This list is taken from
// www.datagenetics.com/blog/september32012/.
constexpr const char* kMostCommonPins[] = {"1212", "1004", "2000", "6969",
                                           "1122", "1313", "2001", "1010"};

// Returns the active set of quick unlock modes.
QuickUnlockModeList ComputeActiveModes(Profile* profile) {
  QuickUnlockModeList modes;

  chromeos::quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      chromeos::quick_unlock::QuickUnlockFactory::GetForProfile(profile);
  if (quick_unlock_storage && quick_unlock_storage->pin_storage()->IsPinSet())
    modes.push_back(quick_unlock_private::QUICK_UNLOCK_MODE_PIN);

  return modes;
}

// Returns true if |a| and |b| contain the same elements. The elements do not
// need to be in the same order.
bool AreModesEqual(const QuickUnlockModeList& a, const QuickUnlockModeList& b) {
  if (a.size() != b.size())
    return false;

  // This is a slow comparison algorithm, but the number of entries in |a| and
  // |b| will always be very low (0-3 items) so it doesn't matter.
  for (size_t i = 0; i < a.size(); ++i) {
    if (!base::ContainsValue(b, a[i]))
      return false;
  }

  return true;
}

bool IsPinNumeric(const std::string& pin) {
  return std::all_of(pin.begin(), pin.end(), ::isdigit);
}

// Reads and sanitizes the pin length policy.
// Returns the minimum and maximum required pin lengths.
// - minimum must be at least 1.
// - maximum must be at least |min_length|, or 0.
std::pair<int, int> GetSanitizedPolicyPinMinMaxLength(
    PrefService* pref_service) {
  int min_length =
      std::max(pref_service->GetInteger(prefs::kPinUnlockMinimumLength), 1);
  int max_length = pref_service->GetInteger(prefs::kPinUnlockMaximumLength);
  max_length = max_length > 0 ? std::max(max_length, min_length) : 0;

  DCHECK_GE(min_length, 1);
  DCHECK_GE(max_length, 0);
  return std::make_pair(min_length, max_length);
}

// Checks whether a given |pin| has any problems given the PIN min/max policies
// in |pref_service|. Returns CREDENTIAL_PROBLEM_NONE if |pin| has no problems,
// or another CREDENTIAL_PROBLEM_ enum value to indicate the detected problem.
CredentialProblem GetCredentialProblemForPin(const std::string& pin,
                                             PrefService* pref_service) {
  int min_length;
  int max_length;
  std::tie(min_length, max_length) =
      GetSanitizedPolicyPinMinMaxLength(pref_service);

  // Check if the PIN is shorter than the minimum specified length.
  if (pin.size() < static_cast<size_t>(min_length))
    return CredentialProblem::CREDENTIAL_PROBLEM_TOO_SHORT;

  // If the maximum specified length is zero, there is no maximum length.
  // Otherwise check if the PIN is longer than the maximum specified length.
  if (max_length != 0 && pin.size() > static_cast<size_t>(max_length))
    return CredentialProblem::CREDENTIAL_PROBLEM_TOO_LONG;

  return CredentialProblem::CREDENTIAL_PROBLEM_NONE;
}

// Checks if a given |pin| is weak or not. A PIN is considered weak if it:
// a) is on this list - www.datagenetics.com/blog/september32012/
// b) has all the same digits
// c) each digit is one larger than the previous digit
// d) each digit is one smaller than the previous digit
// Note: A 9 followed by a 0 is not considered increasing, and a 0 followed by
// a 9 is not considered decreasing.
bool IsPinDifficultEnough(const std::string& pin) {
  // If the pin length is |kMinLengthForNonWeakPin| or less, there is no need to
  // check for same character and increasing pin.
  if (pin.size() <= kMinLengthForNonWeakPin)
    return true;

  // Check if it is on the list of most common PINs.
  if (base::ContainsValue(kMostCommonPins, pin))
    return false;

  // Check for same digits, increasing and decreasing PIN simultaneously.
  bool is_same = true;
  // TODO(sammiequon): Should longer PINs (5+) be still subjected to this?
  bool is_increasing = true;
  bool is_decreasing = true;
  for (size_t i = 1; i < pin.length(); ++i) {
    const char previous = pin[i - 1];
    const char current = pin[i];

    is_same = is_same && (current == previous);
    is_increasing = is_increasing && (current == previous + 1);
    is_decreasing = is_decreasing && (current == previous - 1);
  }

  // PIN is considered weak if any of these conditions is met.
  if (is_same || is_increasing || is_decreasing)
    return false;

  return true;
}

}  // namespace

// quickUnlockPrivate.getAvailableModes

QuickUnlockPrivateGetAvailableModesFunction::
    QuickUnlockPrivateGetAvailableModesFunction()
    : chrome_details_(this) {}

QuickUnlockPrivateGetAvailableModesFunction::
    ~QuickUnlockPrivateGetAvailableModesFunction() {}

ExtensionFunction::ResponseAction
QuickUnlockPrivateGetAvailableModesFunction::Run() {
  // TODO(jdufault): Check for policy and do not return PIN if policy makes it
  // unavailable. See crbug.com/612271.
  const QuickUnlockModeList modes = {
      quick_unlock_private::QUICK_UNLOCK_MODE_PIN};

  return RespondNow(ArgumentList(GetAvailableModes::Results::Create(modes)));
}

// quickUnlockPrivate.getActiveModes

QuickUnlockPrivateGetActiveModesFunction::
    QuickUnlockPrivateGetActiveModesFunction()
    : chrome_details_(this) {}

QuickUnlockPrivateGetActiveModesFunction::
    ~QuickUnlockPrivateGetActiveModesFunction() {}

ExtensionFunction::ResponseAction
QuickUnlockPrivateGetActiveModesFunction::Run() {
  const QuickUnlockModeList modes =
      ComputeActiveModes(chrome_details_.GetProfile());
  return RespondNow(ArgumentList(GetActiveModes::Results::Create(modes)));
}

// quickUnlockPrivate.checkCredential

QuickUnlockPrivateCheckCredentialFunction::
    QuickUnlockPrivateCheckCredentialFunction() {}

QuickUnlockPrivateCheckCredentialFunction::
    ~QuickUnlockPrivateCheckCredentialFunction() {}

ExtensionFunction::ResponseAction
QuickUnlockPrivateCheckCredentialFunction::Run() {
  std::unique_ptr<CheckCredential::Params> params_ =
      CheckCredential::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_);

  auto result = std::make_unique<CredentialCheck>();

  // Only handles pins for now.
  if (params_->mode != QuickUnlockMode::QUICK_UNLOCK_MODE_PIN)
    return RespondNow(ArgumentList(CheckCredential::Results::Create(*result)));

  const std::string& credential = params_->credential;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* pref_service = profile->GetPrefs();
  bool allow_weak = pref_service->GetBoolean(prefs::kPinUnlockWeakPinsAllowed);

  // Check and return the problems.
  std::vector<CredentialProblem>& warnings = result->warnings;
  std::vector<CredentialProblem>& errors = result->errors;
  if (!IsPinNumeric(credential))
    errors.push_back(CredentialProblem::CREDENTIAL_PROBLEM_CONTAINS_NONDIGIT);

  CredentialProblem length_problem =
      GetCredentialProblemForPin(credential, pref_service);
  if (length_problem != CredentialProblem::CREDENTIAL_PROBLEM_NONE)
    errors.push_back(length_problem);

  if (!IsPinDifficultEnough(credential)) {
    auto& log = allow_weak ? warnings : errors;
    log.push_back(CredentialProblem::CREDENTIAL_PROBLEM_TOO_WEAK);
  }

  return RespondNow(ArgumentList(CheckCredential::Results::Create(*result)));
}

QuickUnlockPrivateGetCredentialRequirementsFunction::
    QuickUnlockPrivateGetCredentialRequirementsFunction() {}

QuickUnlockPrivateGetCredentialRequirementsFunction::
    ~QuickUnlockPrivateGetCredentialRequirementsFunction() {}

ExtensionFunction::ResponseAction
QuickUnlockPrivateGetCredentialRequirementsFunction::Run() {
  std::unique_ptr<GetCredentialRequirements::Params> params_ =
      GetCredentialRequirements::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_);

  auto result = std::make_unique<CredentialRequirements>();
  std::tie(result->min_length, result->max_length) =
      GetSanitizedPolicyPinMinMaxLength(
          Profile::FromBrowserContext(browser_context())->GetPrefs());

  return RespondNow(
      ArgumentList(GetCredentialRequirements::Results::Create(*result)));
}

// quickUnlockPrivate.setModes

QuickUnlockPrivateSetModesFunction::QuickUnlockPrivateSetModesFunction()
    : chrome_details_(this) {}

QuickUnlockPrivateSetModesFunction::~QuickUnlockPrivateSetModesFunction() {
  if (extended_authenticator_)
    extended_authenticator_->SetConsumer(nullptr);
}

void QuickUnlockPrivateSetModesFunction::SetAuthenticatorAllocatorForTesting(
    const QuickUnlockPrivateSetModesFunction::AuthenticatorAllocator&
        allocator) {
  authenticator_allocator_ = allocator;
}

void QuickUnlockPrivateSetModesFunction::SetModesChangedEventHandlerForTesting(
    const ModesChangedEventHandler& handler) {
  modes_changed_handler_ = handler;
}

ExtensionFunction::ResponseAction QuickUnlockPrivateSetModesFunction::Run() {
  params_ = SetModes::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_);

  if (params_->modes.size() != params_->credentials.size())
    return RespondNow(Error(kModesAndCredentialsLengthMismatch));

  if (params_->modes.size() > 1)
    return RespondNow(Error(kMultipleModesNotSupported));

  // Verify every credential is valid based on policies.
  PrefService* pref_service =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  bool allow_weak = pref_service->GetBoolean(prefs::kPinUnlockWeakPinsAllowed);

  for (size_t i = 0; i < params_->modes.size(); ++i) {
    if (params_->credentials[i].empty())
      continue;

    if (params_->modes[i] != QuickUnlockMode::QUICK_UNLOCK_MODE_PIN)
      continue;

    if (!IsPinNumeric(params_->credentials[i]))
      return RespondNow(ArgumentList(SetModes::Results::Create(false)));

    CredentialProblem problem =
        GetCredentialProblemForPin(params_->credentials[i], pref_service);
    if (problem != CredentialProblem::CREDENTIAL_PROBLEM_NONE)
      return RespondNow(ArgumentList(SetModes::Results::Create(false)));

    if (!allow_weak && !IsPinDifficultEnough(params_->credentials[i])) {
      return RespondNow(ArgumentList(SetModes::Results::Create(false)));
    }
  }

  const user_manager::User* const user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(
          chrome_details_.GetProfile());
  chromeos::UserContext user_context(user->GetAccountId());
  user_context.SetKey(chromeos::Key(params_->account_password));

  // Alter |user_context| if the user is supervised.
  if (user->GetType() == user_manager::USER_TYPE_SUPERVISED) {
    user_context = chromeos::ChromeUserManager::Get()
                       ->GetSupervisedUserManager()
                       ->GetAuthentication()
                       ->TransformKey(user_context);
  }

  // Lazily allocate the authenticator. We do this here, instead of in the ctor,
  // so that tests can install a fake.
  DCHECK(!extended_authenticator_);
  if (authenticator_allocator_)
    extended_authenticator_ = authenticator_allocator_.Run(this);
  else
    extended_authenticator_ = chromeos::ExtendedAuthenticator::Create(this);

  // The extension function needs to stay alive while the authenticator is
  // running the password check. Add a ref before the authenticator starts, and
  // remove the ref after it invokes one of the OnAuth* callbacks. The PostTask
  // call applies ref management to the extended_authenticator_ instance and not
  // to the extension function instance, which is why the manual ref management
  // is needed.
  AddRef();

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&chromeos::ExtendedAuthenticator::AuthenticateToCheck,
                     extended_authenticator_.get(), user_context,
                     base::Closure()));

  return RespondLater();
}

void QuickUnlockPrivateSetModesFunction::OnAuthFailure(
    const chromeos::AuthFailure& error) {
  Respond(ArgumentList(SetModes::Results::Create(false)));
  Release();  // Balanced in Run().
}

void QuickUnlockPrivateSetModesFunction::OnAuthSuccess(
    const chromeos::UserContext& user_context) {
  const QuickUnlockModeList initial_modes =
      ComputeActiveModes(chrome_details_.GetProfile());
  ApplyModeChange();
  const QuickUnlockModeList updated_modes =
      ComputeActiveModes(chrome_details_.GetProfile());

  if (!AreModesEqual(initial_modes, updated_modes))
    FireEvent(updated_modes);

  EasyUnlockService::Get(chrome_details_.GetProfile())
      ->HandleUserReauth(user_context);

  Respond(ArgumentList(SetModes::Results::Create(true)));
  Release();  // Balanced in Run().
}

void QuickUnlockPrivateSetModesFunction::ApplyModeChange() {
  // This function is setup so it is easy to add another quick unlock mode while
  // following all of the invariants, which are:
  //
  // 1: If an unlock type is not specified, it should be deactivated.
  // 2: If a credential for an unlock type is empty, it should not be touched.
  // 3: Otherwise, the credential should be set to the new value.

  bool update_pin = true;
  std::string pin_credential;

  // Compute needed changes.
  DCHECK_EQ(params_->credentials.size(), params_->modes.size());
  for (size_t i = 0; i < params_->modes.size(); ++i) {
    const QuickUnlockMode mode = params_->modes[i];
    const std::string& credential = params_->credentials[i];

    if (mode == quick_unlock_private::QUICK_UNLOCK_MODE_PIN) {
      update_pin = !credential.empty();
      pin_credential = credential;
    }
  }

  // Apply changes.
  if (update_pin) {
    Profile* profile = chrome_details_.GetProfile();
    chromeos::quick_unlock::QuickUnlockStorage* quick_unlock_storage =
        chromeos::quick_unlock::QuickUnlockFactory::GetForProfile(profile);

    if (pin_credential.empty()) {
      quick_unlock_storage->pin_storage()->RemovePin();
    } else {
      quick_unlock_storage->pin_storage()->SetPin(pin_credential);
      quick_unlock_storage->MarkStrongAuth();
    }
  }
}

// Triggers a quickUnlockPrivate.onActiveModesChanged change event.
void QuickUnlockPrivateSetModesFunction::FireEvent(
    const QuickUnlockModeList& modes) {
  // Allow unit tests to override how events are raised/handled.
  if (modes_changed_handler_) {
    modes_changed_handler_.Run(modes);
    return;
  }

  std::unique_ptr<base::ListValue> args = OnActiveModesChanged::Create(modes);
  auto event = std::make_unique<Event>(
      events::QUICK_UNLOCK_PRIVATE_ON_ACTIVE_MODES_CHANGED,
      OnActiveModesChanged::kEventName, std::move(args));
  EventRouter::Get(browser_context())->BroadcastEvent(std::move(event));
}

}  // namespace extensions
