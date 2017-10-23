// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/settings_private_api.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_delegate.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_delegate_factory.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "content/public/common/page_zoom.h"
#include "extensions/browser/extension_function_registry.h"

namespace {
  const char kDelegateIsNull[] = "delegate is null";
}

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// SettingsPrivateSetPrefFunction
////////////////////////////////////////////////////////////////////////////////

SettingsPrivateSetPrefFunction::~SettingsPrivateSetPrefFunction() {
}

ExtensionFunction::ResponseAction SettingsPrivateSetPrefFunction::Run() {
  std::unique_ptr<api::settings_private::SetPref::Params> parameters =
      api::settings_private::SetPref::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  SettingsPrivateDelegate* delegate =
      SettingsPrivateDelegateFactory::GetForBrowserContext(browser_context());
  if (delegate == nullptr)
    return RespondNow(Error(kDelegateIsNull));

  PrefsUtil::SetPrefResult result =
      delegate->SetPref(parameters->name, parameters->value.get());
  switch (result) {
    case PrefsUtil::SUCCESS:
      return RespondNow(OneArgument(base::MakeUnique<base::Value>(true)));
    case PrefsUtil::PREF_NOT_MODIFIABLE:
      // Not an error, but return false to indicate setting the pref failed.
      return RespondNow(OneArgument(base::MakeUnique<base::Value>(false)));
    case PrefsUtil::PREF_NOT_FOUND:
      return RespondNow(Error("Pref not found: *", parameters->name));
    case PrefsUtil::PREF_TYPE_MISMATCH:
      return RespondNow(Error("Incorrect type used for value of pref *",
                              parameters->name));
    case PrefsUtil::PREF_TYPE_UNSUPPORTED:
      return RespondNow(Error("Unsupported type used for value of pref *",
                              parameters->name));
  }
  NOTREACHED();
  return RespondNow(OneArgument(base::MakeUnique<base::Value>(false)));
}

////////////////////////////////////////////////////////////////////////////////
// SettingsPrivateGetAllPrefsFunction
////////////////////////////////////////////////////////////////////////////////

SettingsPrivateGetAllPrefsFunction::~SettingsPrivateGetAllPrefsFunction() {
}

ExtensionFunction::ResponseAction SettingsPrivateGetAllPrefsFunction::Run() {
  SettingsPrivateDelegate* delegate =
      SettingsPrivateDelegateFactory::GetForBrowserContext(browser_context());

  if (delegate == nullptr)
    return RespondNow(Error(kDelegateIsNull));
  else
    return RespondNow(OneArgument(delegate->GetAllPrefs()));
}

////////////////////////////////////////////////////////////////////////////////
// SettingsPrivateGetPrefFunction
////////////////////////////////////////////////////////////////////////////////

SettingsPrivateGetPrefFunction::~SettingsPrivateGetPrefFunction() {
}

ExtensionFunction::ResponseAction SettingsPrivateGetPrefFunction::Run() {
  std::unique_ptr<api::settings_private::GetPref::Params> parameters =
      api::settings_private::GetPref::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  SettingsPrivateDelegate* delegate =
      SettingsPrivateDelegateFactory::GetForBrowserContext(browser_context());
  if (delegate == nullptr)
    return RespondNow(Error(kDelegateIsNull));

  std::unique_ptr<base::Value> value = delegate->GetPref(parameters->name);
  if (value->is_none())
    return RespondNow(Error("Pref * does not exist", parameters->name));
  else
    return RespondNow(OneArgument(std::move(value)));
}

////////////////////////////////////////////////////////////////////////////////
// SettingsPrivateGetDefaultZoomFunction
////////////////////////////////////////////////////////////////////////////////

SettingsPrivateGetDefaultZoomFunction::
    ~SettingsPrivateGetDefaultZoomFunction() {
}

ExtensionFunction::ResponseAction
    SettingsPrivateGetDefaultZoomFunction::Run() {
  SettingsPrivateDelegate* delegate =
      SettingsPrivateDelegateFactory::GetForBrowserContext(browser_context());

  if (delegate == nullptr)
    return RespondNow(Error(kDelegateIsNull));
  else
    return RespondNow(OneArgument(delegate->GetDefaultZoom()));
}

////////////////////////////////////////////////////////////////////////////////
// SettingsPrivateSetDefaultZoomFunction
////////////////////////////////////////////////////////////////////////////////

SettingsPrivateSetDefaultZoomFunction::
    ~SettingsPrivateSetDefaultZoomFunction() {
}

ExtensionFunction::ResponseAction
    SettingsPrivateSetDefaultZoomFunction::Run() {
  std::unique_ptr<api::settings_private::SetDefaultZoom::Params> parameters =
      api::settings_private::SetDefaultZoom::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  SettingsPrivateDelegate* delegate =
      SettingsPrivateDelegateFactory::GetForBrowserContext(browser_context());
  if (delegate == nullptr)
    return RespondNow(Error(kDelegateIsNull));

  delegate->SetDefaultZoom(parameters->zoom);
  return RespondNow(OneArgument(base::MakeUnique<base::Value>(true)));
}

}  // namespace extensions
