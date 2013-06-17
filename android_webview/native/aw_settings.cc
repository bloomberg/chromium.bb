// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_settings.h"

#include "android_webview/browser/renderer_host/aw_render_view_host_ext.h"
#include "android_webview/native/aw_contents.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "jni/AwSettings_jni.h"
#include "webkit/common/user_agent/user_agent.h"
#include "webkit/common/webpreferences.h"
#include "webkit/glue/webkit_glue.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace android_webview {

AwSettings::AwSettings(JNIEnv* env, jobject obj, jint web_contents)
    : WebContentsObserver(
          reinterpret_cast<content::WebContents*>(web_contents)),
      aw_settings_(env, obj) {
}

AwSettings::~AwSettings() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> scoped_obj = aw_settings_.get(env);
  jobject obj = scoped_obj.obj();
  if (!obj) return;
  Java_AwSettings_nativeAwSettingsGone(env, obj,
                                       reinterpret_cast<jint>(this));
}

void AwSettings::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

AwRenderViewHostExt* AwSettings::GetAwRenderViewHostExt() {
  if (!web_contents()) return NULL;
  AwContents* contents = AwContents::FromWebContents(web_contents());
  if (!contents) return NULL;
  return contents->render_view_host_ext();
}

void AwSettings::ResetScrollAndScaleState(JNIEnv* env, jobject obj) {
  AwRenderViewHostExt* rvhe = GetAwRenderViewHostExt();
  if (!rvhe) return;
  rvhe->ResetScrollAndScaleState();
}

void AwSettings::UpdateEverything() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  ScopedJavaLocalRef<jobject> scoped_obj = aw_settings_.get(env);
  jobject obj = scoped_obj.obj();
  if (!obj) return;
  // Grab the lock and call UpdateEverythingLocked.
  Java_AwSettings_updateEverything(env, obj);
}

void AwSettings::UpdateEverythingLocked(JNIEnv* env, jobject obj) {
  UpdateInitialPageScaleLocked(env, obj);
  UpdateWebkitPreferencesLocked(env, obj);
  UpdateUserAgentLocked(env, obj);
  ResetScrollAndScaleState(env, obj);
  UpdatePreferredSizeMode();
  UpdateFormDataPreferencesLocked(env, obj);
}

void AwSettings::UpdateUserAgentLocked(JNIEnv* env, jobject obj) {
  if (!web_contents()) return;

  ScopedJavaLocalRef<jstring> str =
      Java_AwSettings_getUserAgentLocked(env, obj);
  bool ua_overidden = str.obj() != NULL;

  if (ua_overidden) {
    std::string override = base::android::ConvertJavaStringToUTF8(str);
    web_contents()->SetUserAgentOverride(override);
  }

  const content::NavigationController& controller =
      web_contents()->GetController();
  for (int i = 0; i < controller.GetEntryCount(); ++i)
    controller.GetEntryAtIndex(i)->SetIsOverridingUserAgent(ua_overidden);
}

void AwSettings::UpdateWebkitPreferencesLocked(JNIEnv* env, jobject obj) {
  if (!web_contents()) return;
  AwRenderViewHostExt* render_view_host_ext = GetAwRenderViewHostExt();
  if (!render_view_host_ext) return;

  content::RenderViewHost* render_view_host =
      web_contents()->GetRenderViewHost();
  if (!render_view_host) return;
  WebPreferences prefs = render_view_host->GetWebkitPreferences();

  prefs.text_autosizing_enabled =
      Java_AwSettings_getTextAutosizingEnabledLocked(env, obj);

  int text_size_percent = Java_AwSettings_getTextSizePercentLocked(env, obj);
  if (prefs.text_autosizing_enabled) {
    prefs.font_scale_factor = text_size_percent / 100.0f;
    prefs.force_enable_zoom = text_size_percent >= 130;
    // Use the default zoom level value when Text Autosizer is turned on.
    render_view_host_ext->SetTextZoomLevel(0);
  } else {
    prefs.force_enable_zoom = false;
    render_view_host_ext->SetTextZoomLevel(webkit_glue::ZoomFactorToZoomLevel(
        text_size_percent / 100.0f));
  }

  prefs.standard_font_family_map[webkit_glue::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getStandardFontFamilyLocked(env, obj));

  prefs.fixed_font_family_map[webkit_glue::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getFixedFontFamilyLocked(env, obj));

  prefs.sans_serif_font_family_map[webkit_glue::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getSansSerifFontFamilyLocked(env, obj));

  prefs.serif_font_family_map[webkit_glue::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getSerifFontFamilyLocked(env, obj));

  prefs.cursive_font_family_map[webkit_glue::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getCursiveFontFamilyLocked(env, obj));

  prefs.fantasy_font_family_map[webkit_glue::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getFantasyFontFamilyLocked(env, obj));

  prefs.default_encoding = ConvertJavaStringToUTF8(
      Java_AwSettings_getDefaultTextEncodingLocked(env, obj));

  prefs.minimum_font_size = Java_AwSettings_getMinimumFontSizeLocked(env, obj);

  prefs.minimum_logical_font_size =
      Java_AwSettings_getMinimumLogicalFontSizeLocked(env, obj);

  prefs.default_font_size = Java_AwSettings_getDefaultFontSizeLocked(env, obj);

  prefs.default_fixed_font_size =
      Java_AwSettings_getDefaultFixedFontSizeLocked(env, obj);

  // Blink's LoadsImagesAutomatically and ImagesEnabled must be
  // set cris-cross to Android's. See
  // https://code.google.com/p/chromium/issues/detail?id=224317#c26
  prefs.loads_images_automatically =
      Java_AwSettings_getImagesEnabledLocked(env, obj);
  prefs.images_enabled =
      Java_AwSettings_getLoadsImagesAutomaticallyLocked(env, obj);

  prefs.javascript_enabled =
      Java_AwSettings_getJavaScriptEnabledLocked(env, obj);

  prefs.allow_universal_access_from_file_urls =
      Java_AwSettings_getAllowUniversalAccessFromFileURLsLocked(env, obj);

  prefs.allow_file_access_from_file_urls =
      Java_AwSettings_getAllowFileAccessFromFileURLsLocked(env, obj);

  prefs.javascript_can_open_windows_automatically =
      Java_AwSettings_getJavaScriptCanOpenWindowsAutomaticallyLocked(env, obj);

  prefs.supports_multiple_windows =
      Java_AwSettings_getSupportMultipleWindowsLocked(env, obj);

  prefs.plugins_enabled = !Java_AwSettings_getPluginsDisabledLocked(env, obj);

  prefs.application_cache_enabled =
      Java_AwSettings_getAppCacheEnabledLocked(env, obj);

  prefs.local_storage_enabled =
      Java_AwSettings_getDomStorageEnabledLocked(env, obj);

  prefs.databases_enabled = Java_AwSettings_getDatabaseEnabledLocked(env, obj);

  prefs.double_tap_to_zoom_enabled = prefs.use_wide_viewport =
      Java_AwSettings_getUseWideViewportLocked(env, obj);

  prefs.initialize_at_minimum_page_scale =
      Java_AwSettings_getLoadWithOverviewModeLocked(env, obj);

  prefs.user_gesture_required_for_media_playback =
      Java_AwSettings_getMediaPlaybackRequiresUserGestureLocked(env, obj);

  ScopedJavaLocalRef<jstring> url =
      Java_AwSettings_getDefaultVideoPosterURLLocked(env, obj);
  prefs.default_video_poster_url = url.obj() ?
      GURL(ConvertJavaStringToUTF8(url)) : GURL();

  prefs.support_deprecated_target_density_dpi =
      Java_AwSettings_getSupportDeprecatedTargetDensityDPILocked(env, obj);

  render_view_host->UpdateWebkitPreferences(prefs);
}

void AwSettings::UpdateInitialPageScaleLocked(JNIEnv* env, jobject obj) {
  AwRenderViewHostExt* rvhe = GetAwRenderViewHostExt();
  if (!rvhe) return;

  float initial_page_scale_percent =
      Java_AwSettings_getInitialPageScalePercentLocked(env, obj);
  if (initial_page_scale_percent == 0) {
    rvhe->SetInitialPageScale(-1);
  } else {
    float dip_scale = static_cast<float>(
        Java_AwSettings_getDIPScaleLocked(env, obj));
    rvhe->SetInitialPageScale(initial_page_scale_percent / dip_scale / 100.0f);
  }
}

void AwSettings::UpdatePreferredSizeMode() {
  if (web_contents()->GetRenderViewHost()) {
    web_contents()->GetRenderViewHost()->EnablePreferredSizeMode();
  }
}

void AwSettings::UpdateFormDataPreferencesLocked(JNIEnv* env, jobject obj) {
  if (!web_contents()) return;
  AwContents* contents = AwContents::FromWebContents(web_contents());
  if (!contents) return;

  contents->SetSaveFormData(Java_AwSettings_getSaveFormDataLocked(env, obj));
}

void AwSettings::RenderViewCreated(content::RenderViewHost* render_view_host) {
  // A single WebContents can normally have 0 to many RenderViewHost instances
  // associated with it.
  // This is important since there is only one RenderViewHostExt instance per
  // WebContents (and not one RVHExt per RVH, as you might expect) and updating
  // settings via RVHExt only ever updates the 'current' RVH.
  // In android_webview we don't swap out the RVH on cross-site navigations, so
  // we shouldn't have to deal with the multiple RVH per WebContents case. That
  // in turn means that the newly created RVH is always the 'current' RVH
  // (since we only ever go from 0 to 1 RVH instances) and hence the DCHECK.
  DCHECK(web_contents()->GetRenderViewHost() == render_view_host);

  UpdateEverything();
}

void AwSettings::WebContentsDestroyed(content::WebContents* web_contents) {
  delete this;
}

static jint Init(JNIEnv* env,
                 jobject obj,
                 jint web_contents) {
  AwSettings* settings = new AwSettings(env, obj, web_contents);
  return reinterpret_cast<jint>(settings);
}

static jstring GetDefaultUserAgent(JNIEnv* env, jclass clazz) {
  return base::android::ConvertUTF8ToJavaString(
      env, content::GetUserAgent(GURL())).Release();
}

bool RegisterAwSettings(JNIEnv* env) {
  return RegisterNativesImpl(env) >= 0;
}

}  // namespace android_webview
