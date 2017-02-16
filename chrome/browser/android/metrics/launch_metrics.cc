// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/launch_metrics.h"

#include "base/android/jni_string.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "chrome/browser/android/shortcut_info.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/prefs/pref_metrics_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/rappor/public/rappor_utils.h"
#include "components/rappor/rappor_service_impl.h"
#include "content/public/browser/web_contents.h"
#include "jni/LaunchMetrics_jni.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

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
  // Interpolate the legacy ADD_TO_HOMESCREEN source into standalone/shortcut.
  // Unfortunately, we cannot concretely determine whether a standalone add to
  // homescreen source means a full PWA (with service worker) or a site that has
  // a manifest with display: standalone.
  int histogram_source = source;
  if (histogram_source == ShortcutInfo::SOURCE_ADD_TO_HOMESCREEN_DEPRECATED) {
    if (standalone)
      histogram_source = ShortcutInfo::SOURCE_ADD_TO_HOMESCREEN_STANDALONE;
    else
      histogram_source = ShortcutInfo::SOURCE_ADD_TO_HOMESCREEN_SHORTCUT;
  }

  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  if (web_contents &&
      (histogram_source == ShortcutInfo::SOURCE_APP_BANNER ||
       histogram_source == ShortcutInfo::SOURCE_ADD_TO_HOMESCREEN_PWA)) {
    // What a user has installed on the Home screen can become disconnected from
    // what Chrome believes is on the Home screen if the user clears their data.
    // Use the launch as a signal that the shortcut still exists.
    AppBannerSettingsHelper::RecordBannerEvent(
        web_contents, url, url.spec(),
        AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
        base::Time::Now());

    // Tell the Site Engagement Service about this launch as sites recently
    // launched from a shortcut receive a boost to their engagement.
    SiteEngagementService* service = SiteEngagementService::Get(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()));
    service->SetLastShortcutLaunchTime(url);
  }

  std::string rappor_metric_source;
  switch (histogram_source) {
    case ShortcutInfo::SOURCE_ADD_TO_HOMESCREEN_DEPRECATED:
    case ShortcutInfo::SOURCE_ADD_TO_HOMESCREEN_PWA:
    case ShortcutInfo::SOURCE_ADD_TO_HOMESCREEN_STANDALONE:
    case ShortcutInfo::SOURCE_ADD_TO_HOMESCREEN_SHORTCUT:
      rappor_metric_source = "Launch.HomeScreenSource.AddToHomeScreen";
      break;
    case ShortcutInfo::SOURCE_APP_BANNER:
      rappor_metric_source = "Launch.HomeScreenSource.AppBanner";
      break;
    case ShortcutInfo::SOURCE_BOOKMARK_NAVIGATOR_WIDGET:
      rappor_metric_source = "Launch.HomeScreenSource.BookmarkNavigatorWidget";
      break;
    case ShortcutInfo::SOURCE_BOOKMARK_SHORTCUT_WIDGET:
      rappor_metric_source = "Launch.HomeScreenSource.BookmarkShortcutWidget";
      break;
    case ShortcutInfo::SOURCE_NOTIFICATION:
      rappor_metric_source = "Launch.HomeScreenSource.Notification";
      break;
    case ShortcutInfo::SOURCE_UNKNOWN:
    case ShortcutInfo::SOURCE_COUNT:
      rappor_metric_source = "Launch.HomeScreenSource.Unknown";
      break;
  }

  UMA_HISTOGRAM_ENUMERATION("Launch.HomeScreenSource", histogram_source,
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

static void RecordHomePageLaunchMetrics(
    JNIEnv* env,
    const JavaParamRef<jclass>& caller,
    jboolean show_home_button,
    jboolean homepage_is_ntp,
    const JavaParamRef<jstring>& jhomepage_url) {
  GURL homepage_url(base::android::ConvertJavaStringToUTF8(env, jhomepage_url));
  PrefMetricsService::RecordHomePageLaunchMetrics(
      show_home_button,
      homepage_is_ntp,
      homepage_url);
}

};  // namespace metrics
