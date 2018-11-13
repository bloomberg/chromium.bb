// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux/google_apps_handler.h"

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/welcome/nux/bookmark_item.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/onboarding_welcome_resources.h"
#include "components/favicon/core/favicon_service.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
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
    {static_cast<int>(GoogleApps::kNews), "News", "news",
     "https://news.google.com", IDR_NUX_GOOGLE_APPS_NEWS_1X},
    {static_cast<int>(GoogleApps::kTranslate), "Translate", "translate",
     "https://translate.google.com", IDR_NUX_GOOGLE_APPS_TRANSLATE_1X},
    {static_cast<int>(GoogleApps::kChromeWebStore), "Web Store", "web-store",
     "https://chrome.google.com/webstore", IDR_NUX_GOOGLE_APPS_CHROME_STORE_1X},
};

constexpr const int kGoogleAppIconSize = 48;  // Pixels.

GoogleAppsHandler::GoogleAppsHandler() {}

GoogleAppsHandler::~GoogleAppsHandler() {}

void GoogleAppsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "cacheGoogleAppIcon",
      base::BindRepeating(&GoogleAppsHandler::HandleCacheGoogleAppIcon,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getGoogleAppsList",
      base::BindRepeating(&GoogleAppsHandler::HandleGetGoogleAppsList,
                          base::Unretained(this)));
}

void GoogleAppsHandler::HandleCacheGoogleAppIcon(const base::ListValue* args) {
  int appId;
  args->GetInteger(0, &appId);

  const BookmarkItem* selectedApp = NULL;
  for (size_t i = 0; i < base::size(kGoogleApps); i++) {
    if (static_cast<int>(kGoogleApps[i].id) == appId) {
      selectedApp = &kGoogleApps[i];
      break;
    }
  }
  CHECK(selectedApp);  // WebUI should not be able to pass non-existent ID.

  // Preload the favicon cache with Chrome-bundled images. Otherwise, the
  // pre-populated bookmarks don't have favicons and look bad. Favicons are
  // updated automatically when a user visits a site.
  GURL app_url = GURL(selectedApp->url);
  FaviconServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()),
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->MergeFavicon(
          app_url, app_url, favicon_base::IconType::kFavicon,
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              selectedApp->icon),
          gfx::Size(kGoogleAppIconSize, kGoogleAppIconSize));
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

}  // namespace nux
