// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/screenlock_private/screenlock_private_api.h"

#include <vector>

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/screenlock_private.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/image_loader.h"
#include "ui/gfx/image/image.h"

namespace screenlock = extensions::api::screenlock_private;

namespace extensions {

namespace {

const char kNotLockedError[] = "Screen is not currently locked.";
const char kInvalidIconError[] = "Invalid custom icon data.";

ScreenlockBridge::LockHandler::AuthType ToLockHandlerAuthType(
    screenlock::AuthType auth_type) {
  switch (auth_type) {
    case screenlock::AUTH_TYPE_OFFLINEPASSWORD:
      return ScreenlockBridge::LockHandler::OFFLINE_PASSWORD;
    case screenlock::AUTH_TYPE_NUMERICPIN:
      return ScreenlockBridge::LockHandler::NUMERIC_PIN;
    case screenlock::AUTH_TYPE_USERCLICK:
      return ScreenlockBridge::LockHandler::USER_CLICK;
    case screenlock::AUTH_TYPE_NONE:
      break;
  }
  NOTREACHED();
  return ScreenlockBridge::LockHandler::OFFLINE_PASSWORD;
}

screenlock::AuthType FromLockHandlerAuthType(
    ScreenlockBridge::LockHandler::AuthType auth_type) {
  switch (auth_type) {
    case ScreenlockBridge::LockHandler::OFFLINE_PASSWORD:
      return screenlock::AUTH_TYPE_OFFLINEPASSWORD;
    case ScreenlockBridge::LockHandler::NUMERIC_PIN:
      return screenlock::AUTH_TYPE_NUMERICPIN;
    case ScreenlockBridge::LockHandler::USER_CLICK:
      return screenlock::AUTH_TYPE_USERCLICK;
    case ScreenlockBridge::LockHandler::ONLINE_SIGN_IN:
      // Apps should treat forced online sign in same as system password.
      return screenlock::AUTH_TYPE_OFFLINEPASSWORD;
    case ScreenlockBridge::LockHandler::EXPAND_THEN_USER_CLICK:
      // This type is used for public sessions, which do not support screen
      // locking.
      NOTREACHED();
      return screenlock::AUTH_TYPE_NONE;
  }
  NOTREACHED();
  return screenlock::AUTH_TYPE_OFFLINEPASSWORD;
}

}  // namespace

ScreenlockPrivateGetLockedFunction::ScreenlockPrivateGetLockedFunction() {}

ScreenlockPrivateGetLockedFunction::~ScreenlockPrivateGetLockedFunction() {}

bool ScreenlockPrivateGetLockedFunction::RunAsync() {
  SetResult(new base::FundamentalValue(ScreenlockBridge::Get()->IsLocked()));
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateSetLockedFunction::ScreenlockPrivateSetLockedFunction() {}

ScreenlockPrivateSetLockedFunction::~ScreenlockPrivateSetLockedFunction() {}

bool ScreenlockPrivateSetLockedFunction::RunAsync() {
  scoped_ptr<screenlock::SetLocked::Params> params(
      screenlock::SetLocked::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->locked)
    ScreenlockBridge::Get()->Lock(GetProfile());
  else
    ScreenlockBridge::Get()->Unlock(GetProfile());
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateShowMessageFunction::ScreenlockPrivateShowMessageFunction() {}

ScreenlockPrivateShowMessageFunction::~ScreenlockPrivateShowMessageFunction() {}

bool ScreenlockPrivateShowMessageFunction::RunAsync() {
  scoped_ptr<screenlock::ShowMessage::Params> params(
      screenlock::ShowMessage::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  ScreenlockBridge::LockHandler* locker =
      ScreenlockBridge::Get()->lock_handler();
  if (locker)
    locker->ShowBannerMessage(params->message);
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateShowCustomIconFunction::
  ScreenlockPrivateShowCustomIconFunction() {}

ScreenlockPrivateShowCustomIconFunction::
  ~ScreenlockPrivateShowCustomIconFunction() {}

bool ScreenlockPrivateShowCustomIconFunction::RunAsync() {
  scoped_ptr<screenlock::ShowCustomIcon::Params> params(
      screenlock::ShowCustomIcon::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  ScreenlockBridge::LockHandler* locker =
      ScreenlockBridge::Get()->lock_handler();
  if (!locker) {
    SetError(kNotLockedError);
    return false;
  }

  const int kMaxButtonIconSize = 40;
  bool has_scale_100P = false;
  std::vector<extensions::ImageLoader::ImageRepresentation> icon_info;
  for (size_t i = 0; i < params->icon.size(); ++i) {
    ui::ScaleFactor scale_factor;
    if (params->icon[i]->scale_factor == 1.) {
      scale_factor = ui::SCALE_FACTOR_100P;
    } else if (params->icon[i]->scale_factor == 2.) {
      scale_factor = ui::SCALE_FACTOR_200P;
    } else {
      continue;
    }

    ExtensionResource resource =
        GetExtension()->GetResource(params->icon[i]->url);
    if (resource.empty())
      continue;

    icon_info.push_back(
        ImageLoader::ImageRepresentation(
            resource,
            ImageLoader::ImageRepresentation::RESIZE_WHEN_LARGER,
            gfx::Size(kMaxButtonIconSize * params->icon[i]->scale_factor,
                      kMaxButtonIconSize * params->icon[i]->scale_factor),
            scale_factor));
    if (scale_factor == ui::SCALE_FACTOR_100P)
      has_scale_100P = true;
  }

  if (!has_scale_100P) {
    SetError(kInvalidIconError);
    return false;
  }

  extensions::ImageLoader* loader = extensions::ImageLoader::Get(GetProfile());
  loader->LoadImagesAsync(
      GetExtension(),
      icon_info,
      base::Bind(&ScreenlockPrivateShowCustomIconFunction::OnImageLoaded,
                 this));
  return true;
}

void ScreenlockPrivateShowCustomIconFunction::OnImageLoaded(
    const gfx::Image& image) {
  ScreenlockBridge::LockHandler* locker =
      ScreenlockBridge::Get()->lock_handler();
  locker->ShowUserPodCustomIcon(
      ScreenlockBridge::GetAuthenticatedUserEmail(GetProfile()),
      image);
  SendResponse(error_.empty());
}

ScreenlockPrivateHideCustomIconFunction::
    ScreenlockPrivateHideCustomIconFunction() {
}

ScreenlockPrivateHideCustomIconFunction::
    ~ScreenlockPrivateHideCustomIconFunction() {
}

bool ScreenlockPrivateHideCustomIconFunction::RunAsync() {
  ScreenlockBridge::LockHandler* locker =
      ScreenlockBridge::Get()->lock_handler();
  if (locker) {
    locker->HideUserPodCustomIcon(
        ScreenlockBridge::GetAuthenticatedUserEmail(GetProfile()));
  } else {
    SetError(kNotLockedError);
  }
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateSetAuthTypeFunction::ScreenlockPrivateSetAuthTypeFunction() {}

ScreenlockPrivateSetAuthTypeFunction::~ScreenlockPrivateSetAuthTypeFunction() {}

bool ScreenlockPrivateSetAuthTypeFunction::RunAsync() {
  scoped_ptr<screenlock::SetAuthType::Params> params(
      screenlock::SetAuthType::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ScreenlockBridge::LockHandler* locker =
      ScreenlockBridge::Get()->lock_handler();
  if (locker) {
    std::string initial_value =
        params->initial_value.get() ? *(params->initial_value.get()) : "";
    locker->SetAuthType(
        ScreenlockBridge::GetAuthenticatedUserEmail(GetProfile()),
        ToLockHandlerAuthType(params->auth_type),
        initial_value);
  } else {
    SetError(kNotLockedError);
  }
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateGetAuthTypeFunction::ScreenlockPrivateGetAuthTypeFunction() {}

ScreenlockPrivateGetAuthTypeFunction::~ScreenlockPrivateGetAuthTypeFunction() {}

bool ScreenlockPrivateGetAuthTypeFunction::RunAsync() {
  ScreenlockBridge::LockHandler* locker =
      ScreenlockBridge::Get()->lock_handler();
  if (locker) {
    ScreenlockBridge::LockHandler::AuthType auth_type = locker->GetAuthType(
        ScreenlockBridge::GetAuthenticatedUserEmail(GetProfile()));
    std::string auth_type_name =
        screenlock::ToString(FromLockHandlerAuthType(auth_type));
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

bool ScreenlockPrivateAcceptAuthAttemptFunction::RunAsync() {
  scoped_ptr<screenlock::AcceptAuthAttempt::Params> params(
      screenlock::AcceptAuthAttempt::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ScreenlockBridge::LockHandler* locker =
      ScreenlockBridge::Get()->lock_handler();
  if (locker) {
    if (params->accept) {
      locker->Unlock(ScreenlockBridge::GetAuthenticatedUserEmail(GetProfile()));
    } else {
      locker->EnableInput();
    }
  } else {
    SetError(kNotLockedError);
  }
  SendResponse(error_.empty());
  return true;
}

ScreenlockPrivateEventRouter::ScreenlockPrivateEventRouter(
    content::BrowserContext* context)
    : browser_context_(context) {
  ScreenlockBridge::Get()->AddObserver(this);
}

ScreenlockPrivateEventRouter::~ScreenlockPrivateEventRouter() {}

void ScreenlockPrivateEventRouter::OnScreenDidLock() {
  DispatchEvent(screenlock::OnChanged::kEventName,
      new base::FundamentalValue(true));
}

void ScreenlockPrivateEventRouter::OnScreenDidUnlock() {
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
  ScreenlockBridge::Get()->RemoveObserver(this);
}

void ScreenlockPrivateEventRouter::OnAuthAttempted(
    ScreenlockBridge::LockHandler::AuthType auth_type,
    const std::string& value) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->AppendString(screenlock::ToString(FromLockHandlerAuthType(auth_type)));
  args->AppendString(value);

  scoped_ptr<extensions::Event> event(new extensions::Event(
      screenlock::OnAuthAttempted::kEventName, args.Pass()));
  extensions::EventRouter::Get(browser_context_)->BroadcastEvent(event.Pass());
}

}  // namespace extensions
