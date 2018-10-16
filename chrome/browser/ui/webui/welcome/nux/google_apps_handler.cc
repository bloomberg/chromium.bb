// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux/google_apps_handler.h"

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "chrome/browser/ui/webui/welcome/nux/bookmark_item.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/favicon/core/favicon_service.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"

namespace nux {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GoogleApps {
  kGmailDoNotUse = 0,  // Deprecated.
  kYouTube = 1,
  kMaps = 2,
  kTranslate = 3,
  kNews = 4,
  kChromeWebStore = 5,
  kCount,
};

const char* kGoogleAppsInteractionHistogram =
    "FirstRun.NewUserExperience.GoogleAppsInteraction";

// Strings in costants not translated because this is an experiment.
// TODO(hcarmona): Translate before wide release.
const BookmarkItem kGoogleApps[] = {
    {static_cast<int>(GoogleApps::kYouTube), "YouTube", "youtube",
     "https://youtube.com", IDR_NUX_GOOGLE_APPS_YOUTUBE_1X},
    {static_cast<int>(GoogleApps::kMaps), "Maps", "maps",
     "https://maps.google.com", IDR_NUX_GOOGLE_APPS_MAPS_1X},
    {static_cast<int>(GoogleApps::kTranslate), "Translate", "translate",
     "https://translate.google.com", IDR_NUX_GOOGLE_APPS_TRANSLATE_1X},
    {static_cast<int>(GoogleApps::kNews), "News", "news",
     "https://news.google.com", IDR_NUX_GOOGLE_APPS_NEWS_1X},
    {static_cast<int>(GoogleApps::kChromeWebStore), "Web Store", "web-store",
     "https://chrome.google.com/webstore", IDR_NUX_GOOGLE_APPS_CHROME_STORE_1X},
};

constexpr const int kGoogleAppIconSize = 48;  // Pixels.

GoogleAppsHandler::GoogleAppsHandler(PrefService* prefs,
                                     favicon::FaviconService* favicon_service)
    : prefs_(prefs), favicon_service_(favicon_service) {}

GoogleAppsHandler::~GoogleAppsHandler() {}

void GoogleAppsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "rejectGoogleApps",
      base::BindRepeating(&GoogleAppsHandler::HandleRejectGoogleApps,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "addGoogleApps",
      base::BindRepeating(&GoogleAppsHandler::HandleAddGoogleApps,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getGoogleAppsList",
      base::BindRepeating(&GoogleAppsHandler::HandleGetGoogleAppsList,
                          base::Unretained(this)));
}

void GoogleAppsHandler::HandleRejectGoogleApps(const base::ListValue* args) {
  UMA_HISTOGRAM_ENUMERATION(kGoogleAppsInteractionHistogram,
                            GoogleAppsInteraction::kNoThanks,
                            GoogleAppsInteraction::kCount);
}

void GoogleAppsHandler::HandleAddGoogleApps(const base::ListValue* args) {
  // Add bookmarks for all selected apps.
  for (size_t i = 0; i < base::size(kGoogleApps); ++i) {
    bool selected = false;
    CHECK(args->GetBoolean(i, &selected));
    if (selected) {
      UMA_HISTOGRAM_ENUMERATION(
          "FirstRun.NewUserExperience.GoogleAppsSelection",
          (GoogleApps)kGoogleApps[i].id, GoogleApps::kCount);
      GURL app_url = GURL(kGoogleApps[i].url);
      // TODO(hcarmona): Add bookmark from JS.

      // Preload the favicon cache with Chrome-bundled images. Otherwise, the
      // pre-populated bookmarks don't have favicons and look bad. Favicons are
      // updated automatically when a user visits a site.
      favicon_service_->MergeFavicon(
          app_url, app_url, favicon_base::IconType::kFavicon,
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              kGoogleApps[i].icon),
          gfx::Size(kGoogleAppIconSize, kGoogleAppIconSize));
    }
  }

  // Enable bookmark bar.
  prefs_->SetBoolean(bookmarks::prefs::kShowBookmarkBar, true);

  UMA_HISTOGRAM_ENUMERATION(kGoogleAppsInteractionHistogram,
                            GoogleAppsInteraction::kGetStarted,
                            GoogleAppsInteraction::kCount);
}

void GoogleAppsHandler::HandleGetGoogleAppsList(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  ResolveJavascriptCallback(
      *callback_id,
      bookmarkItemsToListValue(kGoogleApps, base::size(kGoogleApps)));
}

void GoogleAppsHandler::AddSources(content::WebUIDataSource* html_source,
                                   PrefService* prefs) {
  // Localized strings.
  html_source->AddLocalizedString("noThanks", IDS_NO_THANKS);
  html_source->AddLocalizedString("getStarted",
                                  IDS_NUX_GOOGLE_APPS_GET_STARTED);
  html_source->AddLocalizedString("googleAppsDescription",
                                  IDS_NUX_GOOGLE_APPS_DESCRIPTION);

  // Add required resources.
  html_source->AddResourcePath("apps/nux_google_apps.html",
                               IDR_NUX_GOOGLE_APPS_HTML);
  html_source->AddResourcePath("apps/nux_google_apps.js",
                               IDR_NUX_GOOGLE_APPS_JS);

  html_source->AddResourcePath("apps/nux_google_apps_proxy.html",
                               IDR_NUX_GOOGLE_APPS_PROXY_HTML);
  html_source->AddResourcePath("apps/nux_google_apps_proxy.js",
                               IDR_NUX_GOOGLE_APPS_PROXY_JS);

  html_source->AddResourcePath("apps/apps_chooser.html",
                               IDR_NUX_GOOGLE_APPS_CHOOSER_HTML);
  html_source->AddResourcePath("apps/apps_chooser.js",
                               IDR_NUX_GOOGLE_APPS_CHOOSER_JS);

  // Add icons
  html_source->AddResourcePath("apps/chrome_store_1x.png",
                               IDR_NUX_GOOGLE_APPS_CHROME_STORE_1X);
  html_source->AddResourcePath("apps/chrome_store_2x.png",
                               IDR_NUX_GOOGLE_APPS_CHROME_STORE_2X);
  // TODO: rename and centralize to make it easier to share icons between NUX.
  html_source->AddResourcePath("apps/gmail_1x.png",
                               IDR_NUX_EMAIL_GMAIL_1X);
  html_source->AddResourcePath("apps/gmail_2x.png",
                               IDR_NUX_EMAIL_GMAIL_2X);
  html_source->AddResourcePath("apps/google_apps_1x.png",
                               IDR_NUX_GOOGLE_APPS_LOGO_1X);
  html_source->AddResourcePath("apps/google_apps_2x.png",
                               IDR_NUX_GOOGLE_APPS_LOGO_2X);
  html_source->AddResourcePath("apps/maps_1x.png", IDR_NUX_GOOGLE_APPS_MAPS_1X);
  html_source->AddResourcePath("apps/maps_2x.png", IDR_NUX_GOOGLE_APPS_MAPS_2X);
  html_source->AddResourcePath("apps/news_1x.png", IDR_NUX_GOOGLE_APPS_NEWS_1X);
  html_source->AddResourcePath("apps/news_2x.png", IDR_NUX_GOOGLE_APPS_NEWS_2X);
  html_source->AddResourcePath("apps/translate_1x.png",
                               IDR_NUX_GOOGLE_APPS_TRANSLATE_1X);
  html_source->AddResourcePath("apps/translate_2x.png",
                               IDR_NUX_GOOGLE_APPS_TRANSLATE_2X);
  html_source->AddResourcePath("apps/youtube_1x.png",
                               IDR_NUX_GOOGLE_APPS_YOUTUBE_1X);
  html_source->AddResourcePath("apps/youtube_2x.png",
                               IDR_NUX_GOOGLE_APPS_YOUTUBE_2X);

  // Add constants to loadtime data
  html_source->AddBoolean(
      "bookmark_bar_shown",
      prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar));
  html_source->SetJsonPath("strings.js");
}

}  // namespace nux
