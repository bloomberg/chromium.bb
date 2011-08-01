// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_profile_ui.h"

#include "base/utf_string_conversions.h"
#include "base/time.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/new_profile_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kStringsJsFile[] = "strings.js";
const char kNewProfileJsFile[]  = "new_profile.js";

// Checks if this is the current time within a given date range?
bool InDateRange(double begin, double end) {
  base::Time start_time = base::Time::FromDoubleT(begin);
  base::Time end_time = base::Time::FromDoubleT(end);
  return start_time < base::Time::Now() && end_time > base::Time::Now();
}

///////////////////////////////////////////////////////////////////////////////
//
// NewProfileUIHTMLSource
//
///////////////////////////////////////////////////////////////////////////////

class NewProfileUIHTMLSource : public ChromeWebUIDataSource {
 public:
  explicit NewProfileUIHTMLSource(Profile* profile);

 private:
  ~NewProfileUIHTMLSource() {}
  DISALLOW_COPY_AND_ASSIGN(NewProfileUIHTMLSource);
};

NewProfileUIHTMLSource::NewProfileUIHTMLSource(Profile* profile)
    : ChromeWebUIDataSource(chrome::kChromeUINewProfileHost) {
  AddLocalizedString("title", IDS_NEW_PROFILE_SETUP_TITLE);
  AddLocalizedString("createProfile", IDS_NEW_PROFILE_SETUP_CREATE_PROFILE);
  AddLocalizedString("cancelProfile", IDS_NEW_PROFILE_SETUP_CANCEL_PROFILE);
  AddLocalizedString("profileNameLabel", IDS_NEW_PROFILE_SETUP_NAME_LABEL);
  AddLocalizedString("profileIconLabel", IDS_NEW_PROFILE_SETUP_ICON_LABEL);
  AddLocalizedString("summaryConclusion",
                     IDS_NEW_PROFILE_SETUP_SUMMARY_CONCLUSION);

  const string16 short_product_name =
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
  AddString("summaryTitle", l10n_util::GetStringFUTF16(
      IDS_NEW_PROFILE_SETUP_SUMMARY_TITLE, short_product_name));
  AddString("summaryBody", l10n_util::GetStringFUTF16(
      IDS_NEW_PROFILE_SETUP_SUMMARY_BODY, short_product_name));

  // If the user has preferences for a start and end time for a custom logo,
  // and the time now is between these two times, show the custom logo.
  // TODO(sail): We should also update the logo if the theme changes or
  // if these prefs change.
  // TODO(sail): This code is copy pasted from the NTP code. This should be
  // refactored.
  bool hasCustomLogo =
      profile->GetPrefs()->FindPreference(prefs::kNTPCustomLogoStart) &&
      profile->GetPrefs()->FindPreference(prefs::kNTPCustomLogoEnd) &&
      InDateRange(profile->GetPrefs()->GetDouble(prefs::kNTPCustomLogoStart),
                  profile->GetPrefs()->GetDouble(prefs::kNTPCustomLogoEnd));
  AddString("customlogo", ASCIIToUTF16(hasCustomLogo ? "true" : "false"));
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// NewProfileUI
//
///////////////////////////////////////////////////////////////////////////////

NewProfileUI::NewProfileUI(TabContents* contents) : ChromeWebUI(contents) {
  should_hide_url_ = true;

  NewProfileHandler* handler = new NewProfileHandler();
  AddMessageHandler(handler);
  handler->Attach(this);

  // Set up the chrome://theme/ source.
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  ThemeSource* theme = new ThemeSource(profile);
  profile->GetChromeURLDataManager()->AddDataSource(theme);

  // Set up the new profile source.
  NewProfileUIHTMLSource* html_source =
      new NewProfileUIHTMLSource(GetProfile());
  html_source->set_json_path(kStringsJsFile);
  html_source->add_resource_path(kNewProfileJsFile, IDR_NEW_PROFILE_JS);
  html_source->set_default_resource(IDR_NEW_PROFILE_HTML);
  profile->GetChromeURLDataManager()->AddDataSource(html_source);
}
