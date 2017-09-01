// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_settings.h"

#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/renderer_host/aw_render_view_host_ext.h"
#include "android_webview/common/aw_content_client.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/web_preferences.h"
#include "jni/AwSettings_jni.h"
#include "ui/gfx/font_render_params.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::RendererPreferences;
using content::WebPreferences;

namespace android_webview {

namespace {

void PopulateFixedRendererPreferences(RendererPreferences* prefs) {
  prefs->tap_multiple_targets_strategy =
      content::TAP_MULTIPLE_TARGETS_STRATEGY_NONE;

  // TODO(boliu): Deduplicate with chrome/ code.
  CR_DEFINE_STATIC_LOCAL(
      const gfx::FontRenderParams, params,
      (gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), NULL)));
  prefs->should_antialias_text = params.antialiasing;
  prefs->use_subpixel_positioning = params.subpixel_positioning;
  prefs->hinting = params.hinting;
  prefs->use_autohinter = params.autohinter;
  prefs->use_bitmaps = params.use_bitmaps;
  prefs->subpixel_rendering = params.subpixel_rendering;
}

void PopulateFixedWebPreferences(WebPreferences* web_prefs) {
  web_prefs->shrinks_standalone_images_to_fit = false;
  web_prefs->should_clear_document_background = false;
  web_prefs->viewport_meta_enabled = true;
}

const void* const kAwSettingsUserDataKey = &kAwSettingsUserDataKey;

};  // namespace

class AwSettingsUserData : public base::SupportsUserData::Data {
 public:
  explicit AwSettingsUserData(AwSettings* ptr) : settings_(ptr) {}

  static AwSettings* GetSettings(content::WebContents* web_contents) {
    if (!web_contents)
      return NULL;
    AwSettingsUserData* data = static_cast<AwSettingsUserData*>(
        web_contents->GetUserData(kAwSettingsUserDataKey));
    return data ? data->settings_ : NULL;
  }

 private:
  AwSettings* settings_;
};

AwSettings::AwSettings(JNIEnv* env,
                       jobject obj,
                       content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      renderer_prefs_initialized_(false),
      javascript_can_open_windows_automatically_(false),
      aw_settings_(env, obj) {
  web_contents->SetUserData(kAwSettingsUserDataKey,
                            base::MakeUnique<AwSettingsUserData>(this));
}

AwSettings::~AwSettings() {
  if (web_contents()) {
    web_contents()->SetUserData(kAwSettingsUserDataKey, NULL);
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> scoped_obj = aw_settings_.get(env);
  if (scoped_obj.is_null())
    return;
  Java_AwSettings_nativeAwSettingsGone(env, scoped_obj,
                                       reinterpret_cast<intptr_t>(this));
}

bool AwSettings::GetJavaScriptCanOpenWindowsAutomatically() {
  return javascript_can_open_windows_automatically_;
}

void AwSettings::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

AwSettings* AwSettings::FromWebContents(content::WebContents* web_contents) {
  return AwSettingsUserData::GetSettings(web_contents);
}

AwRenderViewHostExt* AwSettings::GetAwRenderViewHostExt() {
  if (!web_contents())
    return NULL;
  AwContents* contents = AwContents::FromWebContents(web_contents());
  if (!contents)
    return NULL;
  return contents->render_view_host_ext();
}

void AwSettings::ResetScrollAndScaleState(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  AwRenderViewHostExt* rvhe = GetAwRenderViewHostExt();
  if (!rvhe)
    return;
  rvhe->ResetScrollAndScaleState();
}

void AwSettings::UpdateEverything() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  ScopedJavaLocalRef<jobject> scoped_obj = aw_settings_.get(env);
  if (scoped_obj.is_null())
    return;
  // Grab the lock and call UpdateEverythingLocked.
  Java_AwSettings_updateEverything(env, scoped_obj);
}

void AwSettings::UpdateEverythingLocked(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  UpdateInitialPageScaleLocked(env, obj);
  UpdateWebkitPreferencesLocked(env, obj);
  UpdateUserAgentLocked(env, obj);
  ResetScrollAndScaleState(env, obj);
  UpdateFormDataPreferencesLocked(env, obj);
  UpdateRendererPreferencesLocked(env, obj);
  UpdateOffscreenPreRasterLocked(env, obj);
}

void AwSettings::UpdateUserAgentLocked(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  if (!web_contents())
    return;

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

void AwSettings::UpdateWebkitPreferencesLocked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!web_contents())
    return;
  AwRenderViewHostExt* render_view_host_ext = GetAwRenderViewHostExt();
  if (!render_view_host_ext)
    return;

  content::RenderViewHost* render_view_host =
      web_contents()->GetRenderViewHost();
  if (!render_view_host)
    return;
  render_view_host->OnWebkitPreferencesChanged();
}

void AwSettings::UpdateInitialPageScaleLocked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  AwRenderViewHostExt* rvhe = GetAwRenderViewHostExt();
  if (!rvhe)
    return;

  float initial_page_scale_percent =
      Java_AwSettings_getInitialPageScalePercentLocked(env, obj);
  if (initial_page_scale_percent == 0) {
    rvhe->SetInitialPageScale(-1);
  } else {
    float dip_scale =
        static_cast<float>(Java_AwSettings_getDIPScaleLocked(env, obj));
    rvhe->SetInitialPageScale(initial_page_scale_percent / dip_scale / 100.0f);
  }
}

void AwSettings::UpdateFormDataPreferencesLocked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!web_contents())
    return;
  AwContents* contents = AwContents::FromWebContents(web_contents());
  if (!contents)
    return;

  contents->SetSaveFormData(Java_AwSettings_getSaveFormDataLocked(env, obj));
}

void AwSettings::UpdateRendererPreferencesLocked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!web_contents())
    return;

  bool update_prefs = false;
  RendererPreferences* prefs = web_contents()->GetMutableRendererPrefs();

  if (!renderer_prefs_initialized_) {
    PopulateFixedRendererPreferences(prefs);
    renderer_prefs_initialized_ = true;
    update_prefs = true;
  }

  if (prefs->accept_languages.compare(
          AwContentBrowserClient::GetAcceptLangsImpl())) {
    prefs->accept_languages = AwContentBrowserClient::GetAcceptLangsImpl();
    update_prefs = true;
  }

  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  if (update_prefs && host)
    host->SyncRendererPrefs();
}

void AwSettings::UpdateOffscreenPreRasterLocked(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  AwContents* contents = AwContents::FromWebContents(web_contents());
  if (contents) {
    contents->SetOffscreenPreRaster(
        Java_AwSettings_getOffscreenPreRasterLocked(env, obj));
  }
}

void AwSettings::RenderViewHostChanged(content::RenderViewHost* old_host,
                                       content::RenderViewHost* new_host) {
  DCHECK_EQ(new_host, web_contents()->GetRenderViewHost());

  UpdateEverything();
}

void AwSettings::WebContentsDestroyed() {
  delete this;
}

void AwSettings::PopulateWebPreferences(WebPreferences* web_prefs) {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);
  ScopedJavaLocalRef<jobject> scoped_obj = aw_settings_.get(env);
  if (scoped_obj.is_null())
    return;
  // Grab the lock and call PopulateWebPreferencesLocked.
  Java_AwSettings_populateWebPreferences(env, scoped_obj,
                                         reinterpret_cast<jlong>(web_prefs));
}

void AwSettings::PopulateWebPreferencesLocked(JNIEnv* env,
                                              const JavaParamRef<jobject>& obj,
                                              jlong web_prefs_ptr) {
  AwRenderViewHostExt* render_view_host_ext = GetAwRenderViewHostExt();
  if (!render_view_host_ext)
    return;

  WebPreferences* web_prefs = reinterpret_cast<WebPreferences*>(web_prefs_ptr);
  PopulateFixedWebPreferences(web_prefs);

  web_prefs->text_autosizing_enabled =
      Java_AwSettings_getTextAutosizingEnabledLocked(env, obj);

  int text_size_percent = Java_AwSettings_getTextSizePercentLocked(env, obj);
  if (web_prefs->text_autosizing_enabled) {
    web_prefs->font_scale_factor = text_size_percent / 100.0f;
    web_prefs->force_enable_zoom = text_size_percent >= 130;
    // Use the default zoom factor value when Text Autosizer is turned on.
    render_view_host_ext->SetTextZoomFactor(1);
  } else {
    web_prefs->force_enable_zoom = false;
    render_view_host_ext->SetTextZoomFactor(text_size_percent / 100.0f);
  }

  web_prefs->standard_font_family_map[content::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getStandardFontFamilyLocked(env, obj));

  web_prefs->fixed_font_family_map[content::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getFixedFontFamilyLocked(env, obj));

  web_prefs->sans_serif_font_family_map[content::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getSansSerifFontFamilyLocked(env, obj));

  web_prefs->serif_font_family_map[content::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getSerifFontFamilyLocked(env, obj));

  web_prefs->cursive_font_family_map[content::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getCursiveFontFamilyLocked(env, obj));

  web_prefs->fantasy_font_family_map[content::kCommonScript] =
      ConvertJavaStringToUTF16(
          Java_AwSettings_getFantasyFontFamilyLocked(env, obj));

  web_prefs->default_encoding = ConvertJavaStringToUTF8(
      Java_AwSettings_getDefaultTextEncodingLocked(env, obj));

  web_prefs->minimum_font_size =
      Java_AwSettings_getMinimumFontSizeLocked(env, obj);

  web_prefs->minimum_logical_font_size =
      Java_AwSettings_getMinimumLogicalFontSizeLocked(env, obj);

  web_prefs->default_font_size =
      Java_AwSettings_getDefaultFontSizeLocked(env, obj);

  web_prefs->default_fixed_font_size =
      Java_AwSettings_getDefaultFixedFontSizeLocked(env, obj);

  // Blink's LoadsImagesAutomatically and ImagesEnabled must be
  // set cris-cross to Android's. See
  // https://code.google.com/p/chromium/issues/detail?id=224317#c26
  web_prefs->loads_images_automatically =
      Java_AwSettings_getImagesEnabledLocked(env, obj);
  web_prefs->images_enabled =
      Java_AwSettings_getLoadsImagesAutomaticallyLocked(env, obj);

  web_prefs->javascript_enabled =
      Java_AwSettings_getJavaScriptEnabledLocked(env, obj);

  web_prefs->allow_universal_access_from_file_urls =
      Java_AwSettings_getAllowUniversalAccessFromFileURLsLocked(env, obj);

  web_prefs->allow_file_access_from_file_urls =
      Java_AwSettings_getAllowFileAccessFromFileURLsLocked(env, obj);

  javascript_can_open_windows_automatically_ =
      Java_AwSettings_getJavaScriptCanOpenWindowsAutomaticallyLocked(env, obj);

  web_prefs->supports_multiple_windows =
      Java_AwSettings_getSupportMultipleWindowsLocked(env, obj);

  web_prefs->plugins_enabled =
      !Java_AwSettings_getPluginsDisabledLocked(env, obj);

  web_prefs->application_cache_enabled =
      Java_AwSettings_getAppCacheEnabledLocked(env, obj);

  web_prefs->local_storage_enabled =
      Java_AwSettings_getDomStorageEnabledLocked(env, obj);

  web_prefs->databases_enabled =
      Java_AwSettings_getDatabaseEnabledLocked(env, obj);

  web_prefs->wide_viewport_quirk = true;
  web_prefs->use_wide_viewport =
      Java_AwSettings_getUseWideViewportLocked(env, obj);

  web_prefs->force_zero_layout_height =
      Java_AwSettings_getForceZeroLayoutHeightLocked(env, obj);

  const bool zero_layout_height_disables_viewport_quirk =
      Java_AwSettings_getZeroLayoutHeightDisablesViewportQuirkLocked(env, obj);
  web_prefs->viewport_enabled = !(zero_layout_height_disables_viewport_quirk &&
                                  web_prefs->force_zero_layout_height);

  web_prefs->double_tap_to_zoom_enabled =
      Java_AwSettings_supportsDoubleTapZoomLocked(env, obj);

  web_prefs->initialize_at_minimum_page_scale =
      Java_AwSettings_getLoadWithOverviewModeLocked(env, obj);

  web_prefs->autoplay_policy =
      Java_AwSettings_getMediaPlaybackRequiresUserGestureLocked(env, obj)
          ? content::AutoplayPolicy::kUserGestureRequired
          : content::AutoplayPolicy::kNoUserGestureRequired;

  ScopedJavaLocalRef<jstring> url =
      Java_AwSettings_getDefaultVideoPosterURLLocked(env, obj);
  web_prefs->default_video_poster_url =
      url.obj() ? GURL(ConvertJavaStringToUTF8(url)) : GURL();

  bool support_quirks = Java_AwSettings_getSupportLegacyQuirksLocked(env, obj);
  // Please see the corresponding Blink settings for bug references.
  web_prefs->support_deprecated_target_density_dpi = support_quirks;
  web_prefs->use_legacy_background_size_shorthand_behavior = support_quirks;
  web_prefs->viewport_meta_layout_size_quirk = support_quirks;
  web_prefs->viewport_meta_merge_content_quirk = support_quirks;
  web_prefs->viewport_meta_non_user_scalable_quirk = support_quirks;
  web_prefs->viewport_meta_zero_values_quirk = support_quirks;
  web_prefs->clobber_user_agent_initial_scale_quirk = support_quirks;
  web_prefs->ignore_main_frame_overflow_hidden_quirk = support_quirks;
  web_prefs->report_screen_size_in_physical_pixels_quirk = support_quirks;

  web_prefs->resue_global_for_unowned_main_frame =
      Java_AwSettings_getAllowEmptyDocumentPersistenceLocked(env, obj);

  web_prefs->password_echo_enabled =
      Java_AwSettings_getPasswordEchoEnabledLocked(env, obj);
  web_prefs->spatial_navigation_enabled =
      Java_AwSettings_getSpatialNavigationLocked(env, obj);

  bool enable_supported_hardware_accelerated_features =
      Java_AwSettings_getEnableSupportedHardwareAcceleratedFeaturesLocked(env,
                                                                          obj);

  bool accelerated_2d_canvas_enabled_by_switch =
      web_prefs->accelerated_2d_canvas_enabled;
  web_prefs->accelerated_2d_canvas_enabled = true;
  if (!accelerated_2d_canvas_enabled_by_switch ||
      !enable_supported_hardware_accelerated_features) {
    // Any canvas smaller than this will fallback to software. Abusing this
    // slightly to turn canvas off without changing
    // accelerated_2d_canvas_enabled, which also affects compositing mode.
    // Using 100M instead of max int to avoid overflows.
    web_prefs->minimum_accelerated_2d_canvas_size = 100 * 1000 * 1000;
  }
  web_prefs->experimental_webgl_enabled =
      web_prefs->experimental_webgl_enabled &&
      enable_supported_hardware_accelerated_features;

  // If strict mixed content checking is enabled then running should not be
  // allowed.
  DCHECK(!Java_AwSettings_getUseStricMixedContentCheckingLocked(env, obj) ||
         !Java_AwSettings_getAllowRunningInsecureContentLocked(env, obj));
  web_prefs->allow_running_insecure_content =
      Java_AwSettings_getAllowRunningInsecureContentLocked(env, obj);
  web_prefs->strict_mixed_content_checking =
      Java_AwSettings_getUseStricMixedContentCheckingLocked(env, obj);

  web_prefs->fullscreen_supported =
      Java_AwSettings_getFullscreenSupportedLocked(env, obj);
  web_prefs->record_whole_document =
      Java_AwSettings_getRecordFullDocument(env, obj);

  // TODO(jww): This should be removed once sufficient warning has been given of
  // possible API breakage because of disabling insecure use of geolocation.
  web_prefs->allow_geolocation_on_insecure_origins =
      Java_AwSettings_getAllowGeolocationOnInsecureOrigins(env, obj);

  web_prefs->do_not_update_selection_on_mutating_selection_range =
      Java_AwSettings_getDoNotUpdateSelectionOnMutatingSelectionRange(env, obj);

  web_prefs->css_hex_alpha_color_enabled =
      Java_AwSettings_getCSSHexAlphaColorEnabledLocked(env, obj);

  // Keep spellcheck disabled on html elements unless the spellcheck="true"
  // attribute is explicitly specified. This "opt-in" behavior is for backward
  // consistency in apps that use WebView (see crbug.com/652314).
  web_prefs->spellcheck_enabled_by_default = false;

  web_prefs->scroll_top_left_interop_enabled =
      Java_AwSettings_getScrollTopLeftInteropEnabledLocked(env, obj);
}

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& obj,
                  const JavaParamRef<jobject>& web_contents) {
  content::WebContents* contents =
      content::WebContents::FromJavaWebContents(web_contents);
  AwSettings* settings = new AwSettings(env, obj, contents);
  return reinterpret_cast<intptr_t>(settings);
}

static ScopedJavaLocalRef<jstring> GetDefaultUserAgent(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  return base::android::ConvertUTF8ToJavaString(env, GetUserAgent());
}

}  // namespace android_webview
