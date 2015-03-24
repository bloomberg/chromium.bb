// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/settings_private_api.h"

#include "base/values.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "extensions/browser/extension_function_registry.h"

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// SettingsPrivateSetBooleanPrefFunction

SettingsPrivateSetBooleanPrefFunction::
    ~SettingsPrivateSetBooleanPrefFunction() {
}

ExtensionFunction::ResponseAction SettingsPrivateSetBooleanPrefFunction::Run() {
  scoped_ptr<api::settings_private::SetBooleanPref::Params> parameters =
      api::settings_private::SetBooleanPref::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());
  VLOG(1) << "chrome.settingsPrivate.setBooleanPref(" << parameters->name
      << ", " << (!!parameters->value ? "true" : "false") << ")";

  // TODO(orenb): Implement with a real check and not just true.
  return RespondNow(OneArgument(new base::FundamentalValue(true)));
}

////////////////////////////////////////////////////////////////////////////////
// SettingsPrivateSetNumericPrefFunction

SettingsPrivateSetNumericPrefFunction::
    ~SettingsPrivateSetNumericPrefFunction() {
}

ExtensionFunction::ResponseAction SettingsPrivateSetNumericPrefFunction::Run() {
  scoped_ptr<api::settings_private::SetNumericPref::Params> parameters =
      api::settings_private::SetNumericPref::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());
  VLOG(1) << "chrome.settingsPrivate.setNumericPref(" << parameters->name
      << ", " << parameters->value << ")";

  // TODO(orenb): Implement with a real check and not just true.
  return RespondNow(OneArgument(new base::FundamentalValue(true)));
}

////////////////////////////////////////////////////////////////////////////////
// SettingsPrivateSetStringPrefFunction

SettingsPrivateSetStringPrefFunction::
    ~SettingsPrivateSetStringPrefFunction() {
}

ExtensionFunction::ResponseAction SettingsPrivateSetStringPrefFunction::Run() {
  scoped_ptr<api::settings_private::SetStringPref::Params> parameters =
      api::settings_private::SetStringPref::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());
  VLOG(1) << "chrome.settingsPrivate.setStringPref(" << parameters->name
      << ", " << parameters->value << ")";

  // TODO(orenb): Implement with a real check and not just true.
  return RespondNow(OneArgument(new base::FundamentalValue(true)));
}

////////////////////////////////////////////////////////////////////////////////
// SettingsPrivateSetURLPrefFunction

SettingsPrivateSetURLPrefFunction::
    ~SettingsPrivateSetURLPrefFunction() {
}

ExtensionFunction::ResponseAction SettingsPrivateSetURLPrefFunction::Run() {
  scoped_ptr<api::settings_private::SetURLPref::Params> parameters =
      api::settings_private::SetURLPref::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());
  VLOG(1) << "chrome.settingsPrivate.setURLPref(" << parameters->name
      << ", " << parameters->value << ")";

  // TODO(orenb): Implement with a real check and not just true.
  return RespondNow(OneArgument(new base::FundamentalValue(true)));
}

////////////////////////////////////////////////////////////////////////////////
// SettingsPrivateGetAllPrefsFunction

SettingsPrivateGetAllPrefsFunction::~SettingsPrivateGetAllPrefsFunction() {
}

ExtensionFunction::ResponseAction SettingsPrivateGetAllPrefsFunction::Run() {
  // TODO(orenb): Implement with real prefs.
  return RespondNow(OneArgument(new base::FundamentalValue(true)));
}

////////////////////////////////////////////////////////////////////////////////
// SettingsPrivateGetPrefFunction

SettingsPrivateGetPrefFunction::~SettingsPrivateGetPrefFunction() {
}

ExtensionFunction::ResponseAction SettingsPrivateGetPrefFunction::Run() {
  scoped_ptr<api::settings_private::GetPref::Params> parameters =
      api::settings_private::GetPref::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());
  VLOG(1) << "chrome.settingsPrivate.getPref(" << parameters->name << ")";

  // TODO(orenb): Implement with real pref.
  api::settings_private::PrefObject* prefObject =
      new api::settings_private::PrefObject();

  prefObject->type = api::settings_private::PrefType::PREF_TYPE_BOOLEAN;
  prefObject->value.reset(new base::FundamentalValue(true));
  prefObject->source = api::settings_private::PrefSource::PREF_SOURCE_USER;

  return RespondNow(OneArgument(prefObject->ToValue().release()));
}

}  // namespace extensions
