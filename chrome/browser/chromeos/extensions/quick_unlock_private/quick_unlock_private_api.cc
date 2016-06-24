// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/quick_unlock_private/quick_unlock_private_api.h"

#include "chrome/browser/chromeos/login/quick_unlock/pin_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/login/auth/extended_authenticator.h"
#include "chromeos/login/auth/user_context.h"
#include "extensions/browser/event_router.h"

namespace extensions {

namespace quick_unlock_private = api::quick_unlock_private;
namespace SetModes = quick_unlock_private::SetModes;
namespace GetActiveModes = quick_unlock_private::GetActiveModes;
namespace GetAvailableModes = quick_unlock_private::GetAvailableModes;
namespace OnActiveModesChanged = quick_unlock_private::OnActiveModesChanged;
using QuickUnlockMode = quick_unlock_private::QuickUnlockMode;
using QuickUnlockModeList = std::vector<QuickUnlockMode>;

namespace {

const char kModesAndCredentialsLengthMismatch[] =
    "|modes| and |credentials| must have the same number of elements";
const char kMultipleModesNotSupported[] =
    "At most one quick unlock mode can be active.";

// Returns the active set of quick unlock modes.
QuickUnlockModeList ComputeActiveModes(Profile* profile) {
  QuickUnlockModeList modes;

  chromeos::PinStorage* pin_storage =
      chromeos::PinStorageFactory::GetForProfile(profile);
  if (pin_storage->IsPinSet())
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
    if (std::find(b.begin(), b.end(), a[i]) == b.end())
      return false;
  }

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

// quickUnlockPrivate.setModes

QuickUnlockPrivateSetModesFunction::QuickUnlockPrivateSetModesFunction()
    : chrome_details_(this) {}

QuickUnlockPrivateSetModesFunction::~QuickUnlockPrivateSetModesFunction() {}

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
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  if (params_->modes.size() != params_->credentials.size())
    return RespondNow(Error(kModesAndCredentialsLengthMismatch));

  if (params_->modes.size() > 1)
    return RespondNow(Error(kMultipleModesNotSupported));

  user_manager::User* user = chromeos::ProfileHelper::Get()->GetUserByProfile(
      chrome_details_.GetProfile());
  chromeos::UserContext user_context(user->GetAccountId());
  user_context.SetKey(chromeos::Key(params_->account_password));

  // Lazily allocate the authenticator. We do this here, instead of in the ctor,
  // so that tests can install a fake.
  if (authenticator_allocator_.is_null())
    extended_authenticator_ = chromeos::ExtendedAuthenticator::Create(this);
  else
    extended_authenticator_ = authenticator_allocator_.Run(this);

  // The extension function needs to stay alive while the authenticator is
  // running the password check. Add a ref before the authenticator starts, and
  // remove the ref after it invokes one of the OnAuth* callbacks. The PostTask
  // call applies ref management to the extended_authenticator_ instance and not
  // to the extension function instance, which is why the manual ref management
  // is needed.
  AddRef();

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&chromeos::ExtendedAuthenticator::AuthenticateToCheck,
                 extended_authenticator_.get(), user_context, base::Closure()));

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
    chromeos::PinStorage* pin_storage =
        chromeos::PinStorageFactory::GetForProfile(profile);

    if (pin_credential.empty())
      pin_storage->RemovePin();
    else
      pin_storage->SetPin(pin_credential);
  }
}

// Triggers a quickUnlockPrivate.onActiveModesChanged change event.
void QuickUnlockPrivateSetModesFunction::FireEvent(
    const QuickUnlockModeList& modes) {
  // Allow unit tests to override how events are raised/handled.
  if (!modes_changed_handler_.is_null()) {
    modes_changed_handler_.Run(modes);
    return;
  }

  std::unique_ptr<base::ListValue> args = OnActiveModesChanged::Create(modes);
  std::unique_ptr<Event> event(
      new Event(events::QUICK_UNLOCK_PRIVATE_ON_ACTIVE_MODES_CHANGED,
                OnActiveModesChanged::kEventName, std::move(args)));
  EventRouter::Get(browser_context())->BroadcastEvent(std::move(event));
}

}  // namespace extensions
