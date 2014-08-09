// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/first_run_private_api.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/chromeos/first_run/first_run.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_manager/user.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

bool FirstRunPrivateGetLocalizedStringsFunction::RunSync() {
  UMA_HISTOGRAM_COUNTS("CrosFirstRun.DialogShown", 1);
  base::DictionaryValue* localized_strings = new base::DictionaryValue();
  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(GetProfile());
  if (!user->GetGivenName().empty()) {
    localized_strings->SetString(
        "greetingHeader",
        l10n_util::GetStringFUTF16(IDS_FIRST_RUN_GREETING_STEP_HEADER,
                                   user->GetGivenName()));
  } else {
    localized_strings->SetString(
        "greetingHeader",
        l10n_util::GetStringUTF16(IDS_FIRST_RUN_GREETING_STEP_HEADER_GENERAL));
  }
  base::string16 product_name =
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
  localized_strings->SetString(
      "greetingText1",
      l10n_util::GetStringFUTF16(IDS_FIRST_RUN_GREETING_STEP_TEXT_1,
                                 product_name));
  localized_strings->SetString(
      "greetingText2",
      l10n_util::GetStringFUTF16(IDS_FIRST_RUN_GREETING_STEP_TEXT_2,
                                 product_name));
  localized_strings->SetString(
      "greetingButton",
      l10n_util::GetStringUTF16(IDS_FIRST_RUN_GREETING_STEP_BUTTON));
  localized_strings->SetString(
      "closeButton",
      l10n_util::GetStringUTF16(IDS_CLOSE));
  webui::SetFontAndTextDirection(localized_strings);
  SetResult(localized_strings);
  return true;
}

bool FirstRunPrivateLaunchTutorialFunction::RunSync() {
  chromeos::first_run::LaunchTutorial();
  return true;
}
