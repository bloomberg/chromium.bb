// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "chrome/browser/android/banners/app_banner_settings_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "jni/AppBannerManager_jni.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;

namespace {
const char kBannerTag[] = "google-play-id";
}

AppBannerManager::AppBannerManager(JNIEnv* env, jobject obj)
    : MetaTagObserver(kBannerTag),
      weak_java_banner_view_manager_(env, obj) {}

AppBannerManager::~AppBannerManager() {
}

void AppBannerManager::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void AppBannerManager::ReplaceWebContents(JNIEnv* env,
                                          jobject obj,
                                          jobject jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  Observe(web_contents);
}

void AppBannerManager::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // TODO(dfalcantara): Get rid of the current banner.
}

void AppBannerManager::HandleMetaTagContent(const std::string& tag_content,
                                            const GURL& expected_url) {
  DCHECK(web_contents());

  if (!AppBannerSettingsHelper::IsAllowed(web_contents(),
                                          expected_url,
                                          tag_content)) {
    return;
  }

  // TODO(dfalcantara): Send the info to the Java side to begin building the
  //                    app banner.
}

jlong Init(JNIEnv* env, jobject obj) {
  AppBannerManager* manager = new AppBannerManager(env, obj);
  return reinterpret_cast<intptr_t>(manager);
}

jboolean IsEnabled(JNIEnv* env, jclass clazz) {
  return false;

  // TODO(dfalcantara): Enable this when more of the pipeline is checked in.
  // return !CommandLine::ForCurrentProcess()->HasSwitch(
  //     switches::kDisableAppBanners);
}

// Register native methods
bool RegisterAppBannerManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
