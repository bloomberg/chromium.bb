// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome_ui_android.h"

#include <string>

#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/accessibility_util.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/welcome_handler_android.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "ui/base/l10n/l10n_util.h"

const char kProductTourBaseURL[] =
    "http://www.google.com/intl/%s/chrome/browser/mobile/tour/android.html";

const char kPrivacyNoticeBaseURL[] =
    "http://www.google.com/chrome/intl/%s/privacy.html";

WelcomeUI::WelcomeUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://welcome source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIWelcomeHost);
  html_source->SetUseJsonJSFormatV2();

  // Localized strings.
  html_source->AddLocalizedString("title",
      IDS_NEW_TAB_CHROME_WELCOME_PAGE_TITLE);
  html_source->AddLocalizedString("takeATour", IDS_FIRSTRUN_TAKE_TOUR);
  html_source->AddLocalizedString("firstRunSignedInTitle",
      IDS_FIRSTRUN_SIGNED_IN_TITLE);
  html_source->AddLocalizedString("firstRunSignedInDescription",
      IDS_FIRSTRUN_SIGNED_IN_DESCRIPTION);
  html_source->AddLocalizedString("settings", IDS_FIRSTRUN_SETTINGS_LINK);

  std::string locale = g_browser_process->GetApplicationLocale();
  std::string product_tour_url = base::StringPrintf(
      kProductTourBaseURL, locale.c_str());

  if (chrome::android::AccessibilityUtil::IsAccessibilityEnabled())
    product_tour_url += "?talkback";

  html_source->AddString("productTourUrl", product_tour_url);

  TabAndroid* tab = TabAndroid::FromWebContents(web_ui->GetWebContents());
  bool tos_visible = tab && tab->ShouldWelcomePageLinkToTermsOfService();
  html_source->AddBoolean("tosVisible", tos_visible);

  base::string16 tos_html;
  if (tos_visible) {
    std::string privacy_notice_url =
        base::StringPrintf(kPrivacyNoticeBaseURL, locale.c_str());
    tos_html = l10n_util::GetStringFUTF16(
        IDS_FIRSTRUN_TOS_EXPLANATION, base::UTF8ToUTF16(privacy_notice_url));
  }
  html_source->AddString("tosHtml", tos_html);

  // Add required resources.
  html_source->SetJsonPath("strings.js");
  html_source->SetDefaultResource(IDR_ABOUT_WELCOME_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);

  // Add message handlers.
  web_ui->AddMessageHandler(new WelcomeHandler());
}

WelcomeUI::~WelcomeUI() {
}
