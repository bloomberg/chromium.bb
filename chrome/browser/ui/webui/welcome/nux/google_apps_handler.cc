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
#include "ui/base/l10n/l10n_util.h"
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

constexpr const int kGoogleAppIconSize = 48;  // Pixels.

GoogleAppsHandler::GoogleAppsHandler()
    :  // Do not translate icon name as it is not human visible and needs to
       // match CSS.
      google_apps_{{
          {static_cast<int>(GoogleApps::kYouTube),
           l10n_util::GetStringUTF8(
               IDS_ONBOARDING_WELCOME_NUX_GOOGLE_APPS_YOUTUBE),
           "youtube",
           l10n_util::GetStringUTF8(
               IDS_ONBOARDING_WELCOME_NUX_GOOGLE_APPS_YOUTUBE_LINK),
           IDR_NUX_GOOGLE_APPS_YOUTUBE_1X},
          {static_cast<int>(GoogleApps::kMaps),
           l10n_util::GetStringUTF8(
               IDS_ONBOARDING_WELCOME_NUX_GOOGLE_APPS_MAPS),
           "maps",
           l10n_util::GetStringUTF8(
               IDS_ONBOARDING_WELCOME_NUX_GOOGLE_APPS_MAPS_LINK),
           IDR_NUX_GOOGLE_APPS_MAPS_1X},
          {static_cast<int>(GoogleApps::kNews),
           l10n_util::GetStringUTF8(
               IDS_ONBOARDING_WELCOME_NUX_GOOGLE_APPS_NEWS),
           "news",
           l10n_util::GetStringUTF8(
               IDS_ONBOARDING_WELCOME_NUX_GOOGLE_APPS_NEWS_LINK),
           IDR_NUX_GOOGLE_APPS_NEWS_1X},
          {static_cast<int>(GoogleApps::kTranslate),
           l10n_util::GetStringUTF8(
               IDS_ONBOARDING_WELCOME_NUX_GOOGLE_APPS_TRANSLATE),
           "translate",
           l10n_util::GetStringUTF8(
               IDS_ONBOARDING_WELCOME_NUX_GOOGLE_APPS_TRANSLATE_LINK),
           IDR_NUX_GOOGLE_APPS_TRANSLATE_1X},
          {static_cast<int>(GoogleApps::kChromeWebStore),
           l10n_util::GetStringUTF8(
               IDS_ONBOARDING_WELCOME_NUX_GOOGLE_APPS_WEB_STORE),
           "web-store",
           l10n_util::GetStringUTF8(
               IDS_ONBOARDING_WELCOME_NUX_GOOGLE_APPS_WEB_STORE_LINK),
           IDR_NUX_GOOGLE_APPS_CHROME_STORE_1X},
      }} {}

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
  for (size_t i = 0; i < kGoogleAppCount; i++) {
    if (static_cast<int>(google_apps_[i].id) == appId) {
      selectedApp = &google_apps_[i];
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
      bookmarkItemsToListValue(google_apps_.data(), kGoogleAppCount));
}

}  // namespace nux
