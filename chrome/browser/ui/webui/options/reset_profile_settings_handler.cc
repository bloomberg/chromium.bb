// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/reset_profile_settings_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const char kResetProfileSettingsLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=settings_reset_profile_settings";
}  // namespace

namespace options {

ResetProfileSettingsHandler::ResetProfileSettingsHandler() {
}

ResetProfileSettingsHandler::~ResetProfileSettingsHandler() {
}

void ResetProfileSettingsHandler::InitializeHandler() {
  Profile* profile = Profile::FromWebUI(web_ui());
  resetter_.reset(new ProfileResetter(profile));
}

void ResetProfileSettingsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "resetProfileSettingsCommit", IDS_RESET_PROFILE_SETTINGS_COMMIT_BUTTON },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "resetProfileSettingsOverlay",
                IDS_RESET_PROFILE_SETTINGS_TITLE);
  localized_strings->SetString(
      "resetProfileSettingsLearnMoreUrl",
      google_util::StringAppendGoogleLocaleParam(
          kResetProfileSettingsLearnMoreUrl));
}

void ResetProfileSettingsHandler::RegisterMessages() {
  // Setup handlers specific to this panel.
  web_ui()->RegisterMessageCallback("performResetProfileSettings",
      base::Bind(&ResetProfileSettingsHandler::HandleResetProfileSettings,
                 base::Unretained(this)));
}

void ResetProfileSettingsHandler::HandleResetProfileSettings(
    const ListValue* /*value*/) {
  DCHECK(resetter_);
  DCHECK(!resetter_->IsActive());

  resetter_->Reset(
      ProfileResetter::ALL,
      base::Bind(&ResetProfileSettingsHandler::OnResetProfileSettingsDone,
                 AsWeakPtr()));
}

void ResetProfileSettingsHandler::OnResetProfileSettingsDone() {
  web_ui()->CallJavascriptFunction("ResetProfileSettingsOverlay.doneResetting");
}

}  // namespace options
