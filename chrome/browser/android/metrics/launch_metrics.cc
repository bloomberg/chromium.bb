// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/launch_metrics.h"

#include "base/android/jni_string.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/browser_process.h"
#include "components/rappor/rappor_utils.h"
#include "content/public/browser/web_contents.h"
#include "jni/LaunchMetrics_jni.h"
#include "url/gurl.h"

namespace metrics {

enum HomeScreenLaunch {
  HOME_SCREEN_LAUNCH_STANDALONE = 0,
  HOME_SCREEN_LAUNCH_SHORTCUT = 1,
  HOME_SCREEN_LAUNCH_COUNT = 2
};

bool RegisterLaunchMetrics(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static void RecordLaunch(JNIEnv* env,
                         const JavaParamRef<jclass>& caller,
                         jboolean standalone,
                         const JavaParamRef<jstring>& jurl,
                         int source,
                         const JavaParamRef<jobject>& jweb_contents) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  if (web_contents && source == ShortcutInfo::SOURCE_APP_BANNER) {
    // What a user has installed on the Home screen can become disconnected from
    // what Chrome believes is on the Home screen if the user clears their data.
    // Use the launch as a signal that the shortcut still exists.
    AppBannerSettingsHelper::RecordBannerEvent(
        web_contents, url, url.spec(),
        AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
        base::Time::Now());
  }

  std::string rappor_metric_source;
  if (source == ShortcutInfo::SOURCE_ADD_TO_HOMESCREEN)
    rappor_metric_source = "Launch.HomeScreenSource.AddToHomeScreen";
  else if (source == ShortcutInfo::SOURCE_APP_BANNER)
    rappor_metric_source = "Launch.HomeScreenSource.AppBanner";
  else if (source == ShortcutInfo::SOURCE_BOOKMARK_NAVIGATOR_WIDGET)
    rappor_metric_source = "Launch.HomeScreenSource.BookmarkNavigatorWidget";
  else if (source == ShortcutInfo::SOURCE_BOOKMARK_SHORTCUT_WIDGET)
    rappor_metric_source = "Launch.HomeScreenSource.BookmarkShortcutWidget";
  else
    rappor_metric_source = "Launch.HomeScreenSource.Unknown";

  UMA_HISTOGRAM_ENUMERATION("Launch.HomeScreenSource", source,
                            ShortcutInfo::SOURCE_COUNT);

  rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                          rappor_metric_source, url);

  int action = standalone ? HOME_SCREEN_LAUNCH_STANDALONE
                          : HOME_SCREEN_LAUNCH_SHORTCUT;
  std::string rappor_metric_action = standalone ? "Launch.HomeScreen.Standalone"
                                                : "Launch.HomeScreen.Shortcut";

  UMA_HISTOGRAM_ENUMERATION("Launch.HomeScreen", action,
                            HOME_SCREEN_LAUNCH_COUNT);

  rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                          rappor_metric_action, url);
}

};  // namespace metrics
