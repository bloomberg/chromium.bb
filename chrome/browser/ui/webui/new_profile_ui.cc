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
#include "chrome/browser/ui/webui/new_profile_dom_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

static const char kStringsJsFile[] = "strings.js";
static const char kNewProfileJsFile[]  = "new_profile.js";

namespace {

// Is the current time within a given date range?
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

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);

  virtual std::string GetMimeType(const std::string&) const;

  static int PathToIDR(const std::string& path);

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
  AddLocalizedString("summaryTitle", l10n_util::GetStringFUTF16(
      IDS_NEW_PROFILE_SETUP_SUMMARY_TITLE, short_product_name));
  AddLocalizedString("summaryBody", l10n_util::GetStringFUTF16(
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
  AddLocalizedString("customlogo",
                     ASCIIToUTF16(hasCustomLogo ? "true" : "false"));
}

void NewProfileUIHTMLSource::StartDataRequest(const std::string& path,
                                             bool is_incognito,
                                             int request_id) {
  if (path == kStringsJsFile) {
    SendLocalizedStringsAsJSON(request_id);
  } else {
    int idr = path == kNewProfileJsFile ? IDR_NEW_PROFILE_JS :
                                          IDR_NEW_PROFILE_HTML;
    SendFromResourceBundle(request_id, idr);
  }
}

std::string NewProfileUIHTMLSource::GetMimeType(const std::string& path) const {
  if (path == kStringsJsFile || path == kNewProfileJsFile)
    return "application/javascript";

  return "text/html";
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// NewProfileUI
//
///////////////////////////////////////////////////////////////////////////////

NewProfileUI::NewProfileUI(TabContents* contents) : ChromeWebUI(contents) {
  should_hide_url_ = true;

  NewProfileDOMHandler* handler = new NewProfileDOMHandler();
  AddMessageHandler(handler);
  handler->Attach(this);

  // Set up the new profile source.
  NewProfileUIHTMLSource* html_source =
      new NewProfileUIHTMLSource(GetProfile());
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}
