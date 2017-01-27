// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome_win10_ui.h"

#include <string>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/startup/startup_features.h"
#include "chrome/browser/ui/webui/welcome_win10_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// Helper function to check the presence of a key/value inside the query in the
// |url|.
bool UrlContainsKeyValueInQuery(const GURL& url,
                                const std::string& key,
                                const std::string& expected_value) {
  std::string value;
  return net::GetValueForKeyInQuery(url, key, &value) &&
         value == expected_value;
}

bool ShouldShowInlineStyleVariant(const GURL& url) {
  // Can be overridden via a query.
  // TODO(pmonette): Remove these checks when they are no longer needed
  //                 (crbug.com/658918).
  if (UrlContainsKeyValueInQuery(url, "style", "inline"))
    return true;
  if (UrlContainsKeyValueInQuery(url, "style", "sectioned"))
    return false;

  return base::FeatureList::IsEnabled(features::kWelcomeWin10InlineStyle);
}

// Adds all the needed localized strings to |html_source|, depending on
// the value of |is_first_run|.
void AddLocalizedStrings(content::WebUIDataSource* html_source,
                         bool is_first_run) {
  // Only show the "Welcome to Chrome" text on first run.
  int welcome_header_id = is_first_run
                              ? IDS_WIN10_WELCOME_HEADER
                              : IDS_WIN10_WELCOME_HEADER_AFTER_FIRST_RUN;
  html_source->AddLocalizedString("headerText", welcome_header_id);

  html_source->AddLocalizedString("continueText", IDS_WIN10_WELCOME_CONTINUE);

  // Default browser strings.
  html_source->AddLocalizedString("defaultBrowserSubheaderText",
                                  IDS_WIN10_WELCOME_MAKE_DEFAULT_SUBHEADING);
  html_source->AddLocalizedString("openSettingsText",
                                  IDS_WIN10_WELCOME_OPEN_SETTINGS);
  html_source->AddLocalizedString("clickEdgeText",
                                  IDS_WIN10_WELCOME_CLICK_EDGE);
  html_source->AddLocalizedString("clickSelectChrome",
                                  IDS_WIN10_WELCOME_SELECT);
  html_source->AddLocalizedString("webBrowserLabel",
                                  IDS_WIN10_WELCOME_BROWSER_LABEL);
  html_source->AddLocalizedString("microsoftEdgeLabel",
                                  IDS_WIN10_WELCOME_EDGE_LABEL);

  // Taskbar pin strings.
  html_source->AddLocalizedString("pinSubheaderText",
                                  IDS_WIN10_WELCOME_PIN_SUBHEADING);
  html_source->AddLocalizedString("rightClickText",
                                  IDS_WIN10_WELCOME_RIGHT_CLICK_TASKBAR);
  html_source->AddLocalizedString("pinInstructionText",
                                  IDS_WIN10_WELCOME_PIN_INSTRUCTION);
  html_source->AddLocalizedString("pinToTaskbarLabel",
                                  IDS_WIN10_WELCOME_PIN_LABEL);
}

}  // namespace

WelcomeWin10UI::WelcomeWin10UI(content::WebUI* web_ui, const GURL& url)
    : content::WebUIController(web_ui) {
  static const char kCssFilePath[] = "welcome.css";
  static const char kJsFilePath[] = "welcome.js";
  static const char kDefaultFilePath[] = "default.webp";
  static const char kPinFilePath[] = "pin.webp";

  // Remember that the Win10 promo page has been shown.
  g_browser_process->local_state()->SetBoolean(prefs::kHasSeenWin10PromoPage,
                                               true);

  // Determine which variation to show.
  bool is_first_run = !UrlContainsKeyValueInQuery(url, "text", "faster");
  bool is_inline_style = ShouldShowInlineStyleVariant(url);

  web_ui->AddMessageHandler(
      base::MakeUnique<WelcomeWin10Handler>(is_inline_style));

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(url.host());

  AddLocalizedStrings(html_source, is_first_run);

  if (is_inline_style) {
    html_source->AddResourcePath(kCssFilePath, IDR_WELCOME_WIN10_INLINE_CSS);
    html_source->AddResourcePath(kJsFilePath, IDR_WELCOME_WIN10_INLINE_JS);
    html_source->AddResourcePath(kDefaultFilePath,
                                 IDR_WELCOME_WIN10_DEFAULT_SMALL_WEBP);
    html_source->AddResourcePath(kPinFilePath,
                                 IDR_WELCOME_WIN10_PIN_SMALL_WEBP);
    html_source->SetDefaultResource(IDR_WELCOME_WIN10_INLINE_HTML);
  } else {
    html_source->AddResourcePath(kCssFilePath, IDR_WELCOME_WIN10_SECTIONED_CSS);
    html_source->AddResourcePath(kJsFilePath, IDR_WELCOME_WIN10_SECTIONED_JS);
    html_source->AddResourcePath(kDefaultFilePath,
                                 IDR_WELCOME_WIN10_DEFAULT_LARGE_WEBP);
    html_source->AddResourcePath(kPinFilePath,
                                 IDR_WELCOME_WIN10_PIN_LARGE_WEBP);
    html_source->SetDefaultResource(IDR_WELCOME_WIN10_SECTIONED_HTML);
  }

  html_source->AddResourcePath("logo-small.png", IDR_PRODUCT_LOGO_64);
  html_source->AddResourcePath("logo-large.png", IDR_PRODUCT_LOGO_128);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);
}

WelcomeWin10UI::~WelcomeWin10UI() = default;
