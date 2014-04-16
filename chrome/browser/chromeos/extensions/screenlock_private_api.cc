// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/screenlock_private_api.h"

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/common/extensions/api/screenlock_private.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "ui/gfx/image/image.h"

namespace screenlock = extensions::api::screenlock_private;

namespace extensions {

namespace {

const char kNotLockedError[] = "Screen is not currently locked.";

chromeos::LoginDisplay::AuthType ToLoginDisplayAuthType(
    screenlock::AuthType auth_type) {
  switch (auth_type) {
    case screenlock::AUTH_TYPE_OFFLINEPASSWORD:
      return chromeos::LoginDisplay::OFFLINE_PASSWORD;
    case screenlock::AUTH_TYPE_NUMERICPIN:
      return chromeos::LoginDisplay::NUMERIC_PIN;
    case screenlock::AUTH_TYPE_USERCLICK:
      return chromeos::LoginDisplay::USER_CLICK;
    default:
      NOTREACHED();
      return chromeos::LoginDisplay::OFFLINE_PASSWORD;
  }
}

screenlock::AuthType ToScreenlockPrivateAuthType(
    chromeos::LoginDisplay::AuthType auth_type) {
  switch (auth_type) {
    case chromeos::LoginDisplay::OFFLINE_PASSWORD:
      return screenlock::AUTH_TYPE_OFFLINEPASSWORD;
    case chromeos::LoginDisplay::NUMERIC_PIN:
      return screenlock::AUTH_TYPE_NUMERICPIN;
    case chromeos::LoginDisplay::USER_CLICK:
      return screenlock::AUTH_TYPE_USERCLICK;
    case chromeos::LoginDisplay::ONLINE_SIGN_IN:
      // Apps should treat forced online sign in same as system password.
      return screenlock::AUTH_TYPE_OFFLINEPASSWORD;
    default:
      NOTREACHED();
      return screenlock::AUTH_TYPE_OFFLINEPASSWORD;
  }
}

}  // namespace

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
  if (locker)
    locker->ShowBannerMessage(params->message);
  SendResponse(error_.empty());
  return true;
}

static const int kMaxButtonIconSize = 40;

ScreenlockPrivateShowButtonFunction::
  ScreenlockPrivateShowButtonFunction() {}

ScreenlockPrivateShowButtonFunction::
  ~ScreenlockPrivateShowButtonFunction() {}

bool ScreenlockPrivateShowButtonFunction::RunImpl() {
  scoped_ptr<screenlock::ShowButton::Params> params(
      screenlock::ShowButton::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  chromeos::ScreenLocker* locker =
      chromeos::ScreenLocker::default_screen_locker();
  if (!locker) {
    SetError(kNotLockedError);
    SendResponse(false);
    return true;
  }
  extensions::ImageLoader* loader = extensions::ImageLoader::Get(GetProfile());
  loader->LoadImageAsync(
      GetExtension(), GetExtension()->GetResource(params->icon),
      gfx::Size(kMaxButtonIconSize, kMaxButtonIconSize),
      base::Bind(&ScreenlockPrivateShowButtonFunction::OnImageLoaded, this));
  return true;
}

void ScreenlockPrivateShowButtonFunction::OnImageLoaded(
    const gfx::Image& image) {
  chromeos::ScreenLocker* locker =
      chromeos::ScreenLocker::default_screen_locker();
  ScreenlockPrivateEventRouter* router =
      ScreenlockPrivateEventRouter::GetFactoryInstance()->Get(GetProfile());
  const chromeos::User* user =
      chromeos::UserManager::Get()->GetUserByProfile(GetProfile());
  locker->ShowUserPodButton(
      user->email(),
      image,
      base::Bind(&ScreenlockPrivateEventRouter::OnButtonClicked,
                 base::Unretained(router)));
  SendResponse(error_.empty());
}

ScreenlockPrivateHideButtonFunction::ScreenlockPrivateHideButtonFunction() {}

ScreenlockPrivateHideButtonFunction::~ScreenlockPrivateHideButtonFunction() {}

bool ScreenlockPrivateHideButtonFunction::RunImpl() {
  chromeos::ScreenLocker* locker =
      chromeos::ScreenLocker::default_screen_locker();
  if (locker) {
    const chromeos::User* user =
        chromeos::UserManager::Get()->GetUserByProfile(GetProfile());
    locker->HideUserPodButton(user->email());
  } else {
    SetError(kNotLockedError);
  }
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateSetAuthTypeFunction::ScreenlockPrivateSetAuthTypeFunction() {}

ScreenlockPrivateSetAuthTypeFunction::~ScreenlockPrivateSetAuthTypeFunction() {}

bool ScreenlockPrivateSetAuthTypeFunction::RunImpl() {
  scoped_ptr<screenlock::SetAuthType::Params> params(
      screenlock::SetAuthType::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  chromeos::ScreenLocker* locker =
      chromeos::ScreenLocker::default_screen_locker();
  if (locker) {
    std::string initial_value =
        params->initial_value.get() ? *(params->initial_value.get()) : "";
    const chromeos::User* user =
        chromeos::UserManager::Get()->GetUserByProfile(GetProfile());
    locker->SetAuthType(user->email(),
                        ToLoginDisplayAuthType(params->auth_type),
                        initial_value);
  } else {
    SetError(kNotLockedError);
  }
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateGetAuthTypeFunction::ScreenlockPrivateGetAuthTypeFunction() {}

ScreenlockPrivateGetAuthTypeFunction::~ScreenlockPrivateGetAuthTypeFunction() {}

bool ScreenlockPrivateGetAuthTypeFunction::RunImpl() {
  chromeos::ScreenLocker* locker =
      chromeos::ScreenLocker::default_screen_locker();
  if (locker) {
    const chromeos::User* user =
        chromeos::UserManager::Get()->GetUserByProfile(GetProfile());
    chromeos::LoginDisplay::AuthType auth_type =
        locker->GetAuthType(user->email());
    std::string auth_type_name =
        screenlock::ToString(ToScreenlockPrivateAuthType(auth_type));
    SetResult(new base::StringValue(auth_type_name));
  } else {
    SetError(kNotLockedError);
  }
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateAcceptAuthAttemptFunction::
    ScreenlockPrivateAcceptAuthAttemptFunction() {}

ScreenlockPrivateAcceptAuthAttemptFunction::
    ~ScreenlockPrivateAcceptAuthAttemptFunction() {}

bool ScreenlockPrivateAcceptAuthAttemptFunction::RunImpl() {
  scoped_ptr<screenlock::AcceptAuthAttempt::Params> params(
      screenlock::AcceptAuthAttempt::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  chromeos::ScreenLocker* locker =
      chromeos::ScreenLocker::default_screen_locker();
  if (locker) {
    if (params->accept)
      chromeos::ScreenLocker::Hide();
    else
      locker->EnableInput();
  } else {
    SetError(kNotLockedError);
  }
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateEventRouter::ScreenlockPrivateEventRouter(
    content::BrowserContext* context)
    : browser_context_(context) {
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
  if (arg)
    args->Append(arg);
  scoped_ptr<extensions::Event> event(new extensions::Event(
      event_name, args.Pass()));
  extensions::EventRouter::Get(browser_context_)->BroadcastEvent(event.Pass());
}

static base::LazyInstance<extensions::BrowserContextKeyedAPIFactory<
    ScreenlockPrivateEventRouter> > g_factory = LAZY_INSTANCE_INITIALIZER;

// static
extensions::BrowserContextKeyedAPIFactory<ScreenlockPrivateEventRouter>*
ScreenlockPrivateEventRouter::GetFactoryInstance() {
  return g_factory.Pointer();
}

void ScreenlockPrivateEventRouter::Shutdown() {
  chromeos::SessionManagerClient* session_manager =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  if (session_manager->HasObserver(this))
    session_manager->RemoveObserver(this);
}

void ScreenlockPrivateEventRouter::OnButtonClicked() {
  DispatchEvent(screenlock::OnButtonClicked::kEventName, NULL);
}

void ScreenlockPrivateEventRouter::OnAuthAttempted(
    chromeos::LoginDisplay::AuthType auth_type,
    const std::string& value) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->AppendString(
      screenlock::ToString(ToScreenlockPrivateAuthType(auth_type)));
  args->AppendString(value);

  scoped_ptr<extensions::Event> event(new extensions::Event(
      screenlock::OnAuthAttempted::kEventName, args.Pass()));
  extensions::EventRouter::Get(browser_context_)->BroadcastEvent(event.Pass());
}

}  // namespace extensions
