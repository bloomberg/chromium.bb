// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nux_google_apps/webui.h"

#include "base/metrics/field_trial_params.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/nux_google_apps/constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"

namespace nux_google_apps {

void AddSources(content::WebUIDataSource* html_source) {
  // Localized strings.
  html_source->AddLocalizedString("noThanks", IDS_NO_THANKS);
  html_source->AddLocalizedString("getStarted",
                                  IDS_NUX_GOOGLE_APPS_GET_STARTED);
  html_source->AddLocalizedString("nuxDescription",
                                  IDS_NUX_GOOGLE_APPS_DESCRIPTION);

  // Add required resources.
  html_source->AddResourcePath("apps", IDR_NUX_GOOGLE_APPS_HTML);
  html_source->AddResourcePath("apps/nux_google_apps.js",
                               IDR_NUX_GOOGLE_APPS_JS);

  html_source->AddResourcePath("apps/apps_chooser.html",
                               IDR_NUX_GOOGLE_APPS_CHOOSER_HTML);
  html_source->AddResourcePath("apps/apps_chooser.js",
                               IDR_NUX_GOOGLE_APPS_CHOOSER_JS);

  // Add icons
  html_source->AddResourcePath("apps/chrome_store_1x.png",
                               IDR_NUX_GOOGLE_APPS_CHROME_STORE_1X);
  html_source->AddResourcePath("apps/chrome_store_2x.png",
                               IDR_NUX_GOOGLE_APPS_CHROME_STORE_2X);
  html_source->AddResourcePath("apps/gmail_1x.png",
                               IDR_NUX_GOOGLE_APPS_GMAIL_1X);
  html_source->AddResourcePath("apps/gmail_2x.png",
                               IDR_NUX_GOOGLE_APPS_GMAIL_2X);
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
}

}  // namespace nux_google_apps
