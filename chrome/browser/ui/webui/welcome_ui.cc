// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome_ui.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/welcome_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/resources/grit/webui_resources.h"

WelcomeUI::WelcomeUI(content::WebUI* web_ui, const GURL& url)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  // This page is not shown to incognito or guest profiles. If one should end up
  // here, we return, causing a 404-like page.
  if (!profile ||
      profile->GetProfileType() != Profile::ProfileType::REGULAR_PROFILE) {
    return;
  }

  // Store that this profile has been shown the Welcome page.
  profile->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage, true);

  web_ui->AddMessageHandler(base::MakeUnique<WelcomeHandler>(web_ui));

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(url.host());

  // Check URL for variations.
  std::string value;
  bool is_everywhere_variant =
      (net::GetValueForKeyInQuery(url, "variant", &value) &&
       value == "everywhere");

  int header_id = is_everywhere_variant ? IDS_WELCOME_HEADER_AFTER_FIRST_RUN
                                        : IDS_WELCOME_HEADER;
  html_source->AddString("headerText", l10n_util::GetStringUTF16(header_id));

// The subheader describes the browser as being "by Google", so we only show
// it in the "Welcome to Chrome" variant, and only on branded builds.
#if defined(GOOGLE_CHROME_BUILD)
  base::string16 subheader =
      is_everywhere_variant ? base::string16()
                            : l10n_util::GetStringUTF16(IDS_WELCOME_SUBHEADER);
  html_source->AddString("subheaderText", subheader);
#endif

  html_source->AddLocalizedString("descriptionText", IDS_WELCOME_DESCRIPTION);
  html_source->AddLocalizedString("acceptText", IDS_WELCOME_ACCEPT_BUTTON);
  html_source->AddLocalizedString("declineText", IDS_WELCOME_DECLINE_BUTTON);

  html_source->AddResourcePath("welcome.js", IDR_WELCOME_JS);
  html_source->AddResourcePath("welcome.css", IDR_WELCOME_CSS);
  html_source->AddResourcePath("logo.png", IDR_PRODUCT_LOGO_128);
  html_source->AddResourcePath("logo2x.png", IDR_PRODUCT_LOGO_256);
  html_source->AddResourcePath("watermark.svg", IDR_WEBUI_IMAGES_GOOGLE_LOGO);
  html_source->SetDefaultResource(IDR_WELCOME_HTML);

  content::WebUIDataSource::Add(profile, html_source);
}

WelcomeUI::~WelcomeUI() {}
