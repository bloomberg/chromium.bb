// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/screenlock_private_api.h"

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/common/extensions/api/screenlock_private.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "extensions/browser/event_router.h"

namespace screenlock = extensions::api::screenlock_private;

namespace extensions {

ScreenlockPrivateGetLockedFunction::ScreenlockPrivateGetLockedFunction() {}

ScreenlockPrivateGetLockedFunction::~ScreenlockPrivateGetLockedFunction() {}

bool ScreenlockPrivateGetLockedFunction::RunImpl() {
  bool locked = false;
  chromeos::ScreenLocker* locker =
      chromeos::ScreenLocker::default_screen_locker();
  if (locker)
    locked = locker->locked();
  SetResult(new base::FundamentalValue(locked));
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateSetLockedFunction::ScreenlockPrivateSetLockedFunction() {}

ScreenlockPrivateSetLockedFunction::~ScreenlockPrivateSetLockedFunction() {}

bool ScreenlockPrivateSetLockedFunction::RunImpl() {
  scoped_ptr<screenlock::SetLocked::Params> params(
      screenlock::SetLocked::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->locked) {
    chromeos::SessionManagerClient* session_manager =
        chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
    session_manager->RequestLockScreen();
  } else {
    chromeos::ScreenLocker* locker =
        chromeos::ScreenLocker::default_screen_locker();
    if (locker)
      chromeos::ScreenLocker::Hide();
  }
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateShowMessageFunction::ScreenlockPrivateShowMessageFunction() {}

ScreenlockPrivateShowMessageFunction::~ScreenlockPrivateShowMessageFunction() {}

bool ScreenlockPrivateShowMessageFunction::RunImpl() {
  scoped_ptr<screenlock::ShowMessage::Params> params(
      screenlock::ShowMessage::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  chromeos::ScreenLocker* locker =
      chromeos::ScreenLocker::default_screen_locker();
  if (!locker) {
    SendResponse(error_.empty());
    return true;
  }
  locker->ShowBannerMessage(params->message);
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateEventRouter::ScreenlockPrivateEventRouter(Profile* profile)
    : profile_(profile) {
  chromeos::SessionManagerClient* session_manager =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  if (!session_manager->HasObserver(this))
    session_manager->AddObserver(this);
}

ScreenlockPrivateEventRouter::~ScreenlockPrivateEventRouter() {}

void ScreenlockPrivateEventRouter::ScreenIsLocked() {
  DispatchEvent(screenlock::OnChanged::kEventName,
      new base::FundamentalValue(true));
}

void ScreenlockPrivateEventRouter::ScreenIsUnlocked() {
  DispatchEvent(screenlock::OnChanged::kEventName,
      new base::FundamentalValue(false));
}

void ScreenlockPrivateEventRouter::DispatchEvent(
    const std::string& event_name,
    base::Value* arg) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(arg);
  scoped_ptr<extensions::Event> event(new extensions::Event(
      event_name, args.Pass()));
  extensions::ExtensionSystem::Get(profile_)->event_router()->
      BroadcastEvent(event.Pass());
}

static base::LazyInstance<extensions::ProfileKeyedAPIFactory<
    ScreenlockPrivateEventRouter> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
extensions::ProfileKeyedAPIFactory<ScreenlockPrivateEventRouter>*
ScreenlockPrivateEventRouter::GetFactoryInstance() {
  return &g_factory.Get();
}

void ScreenlockPrivateEventRouter::Shutdown() {
  chromeos::SessionManagerClient* session_manager =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  if (session_manager->HasObserver(this))
    session_manager->RemoveObserver(this);
}

}  // namespace extensions
