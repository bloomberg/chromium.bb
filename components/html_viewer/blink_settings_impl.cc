// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/blink_settings_impl.h"

#include "base/command_line.h"
#include "components/html_viewer/web_preferences.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebNetworkStateNotifier.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "ui/base/touch/touch_device.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/gfx/font_render_params.h"

#if defined(OS_ANDROID)
#include "url/gurl.h"
#endif

#if defined(OS_LINUX)
#include "third_party/WebKit/public/web/linux/WebFontRendering.h"
#endif

namespace html_viewer {

// TODO(rjkroege): Update this code once http://crbug.com/481108 has been
// completed and the WebSettings parameter space has been reduced.
//
// The code inside the anonymous namespace has been copied from content
// and is largely unchanged here. My assumption is that this is a
// temporary measure until 481108 is resolved.
namespace {

// Command line switch string to disable experimental WebGL.
const char kDisableWebGLSwitch[] = "disable-webgl";

blink::WebConnectionType NetConnectionTypeToWebConnectionType(
    net::NetworkChangeNotifier::ConnectionType net_type) {
  switch (net_type) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
      return blink::WebConnectionTypeUnknown;
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      return blink::WebConnectionTypeEthernet;
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return blink::WebConnectionTypeWifi;
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      return blink::WebConnectionTypeNone;
    case net::NetworkChangeNotifier::CONNECTION_2G:
    case net::NetworkChangeNotifier::CONNECTION_3G:
    case net::NetworkChangeNotifier::CONNECTION_4G:
      return blink::WebConnectionTypeCellular;
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return blink::WebConnectionTypeBluetooth;
  }
  NOTREACHED();
  return blink::WebConnectionTypeNone;
}

typedef void (*SetFontFamilyWrapper)(blink::WebSettings*,
                                     const base::string16&,
                                     UScriptCode);

// If |scriptCode| is a member of a family of "similar" script codes, returns
// the script code in that family that is used by WebKit for font selection
// purposes.  For example, USCRIPT_KATAKANA_OR_HIRAGANA and USCRIPT_JAPANESE are
// considered equivalent for the purposes of font selection.  WebKit uses the
// script code USCRIPT_KATAKANA_OR_HIRAGANA.  So, if |scriptCode| is
// USCRIPT_JAPANESE, the function returns USCRIPT_KATAKANA_OR_HIRAGANA.  WebKit
// uses different scripts than the ones in Chrome pref names because the version
// of ICU included on certain ports does not have some of the newer scripts.  If
// |scriptCode| is not a member of such a family, returns |scriptCode|.
UScriptCode GetScriptForWebSettings(UScriptCode scriptCode) {
  switch (scriptCode) {
    case USCRIPT_HIRAGANA:
    case USCRIPT_KATAKANA:
    case USCRIPT_JAPANESE:
      return USCRIPT_KATAKANA_OR_HIRAGANA;
    case USCRIPT_KOREAN:
      return USCRIPT_HANGUL;
    default:
      return scriptCode;
  }
}

void SetStandardFontFamilyWrapper(blink::WebSettings* settings,
                                  const base::string16& font,
                                  UScriptCode script) {
  settings->setStandardFontFamily(font, script);
}

void SetFixedFontFamilyWrapper(blink::WebSettings* settings,
                               const base::string16& font,
                               UScriptCode script) {
  settings->setFixedFontFamily(font, script);
}

void SetSerifFontFamilyWrapper(blink::WebSettings* settings,
                               const base::string16& font,
                               UScriptCode script) {
  settings->setSerifFontFamily(font, script);
}

void SetSansSerifFontFamilyWrapper(blink::WebSettings* settings,
                                   const base::string16& font,
                                   UScriptCode script) {
  settings->setSansSerifFontFamily(font, script);
}

void SetCursiveFontFamilyWrapper(blink::WebSettings* settings,
                                 const base::string16& font,
                                 UScriptCode script) {
  settings->setCursiveFontFamily(font, script);
}

void SetFantasyFontFamilyWrapper(blink::WebSettings* settings,
                                 const base::string16& font,
                                 UScriptCode script) {
  settings->setFantasyFontFamily(font, script);
}

void SetPictographFontFamilyWrapper(blink::WebSettings* settings,
                                    const base::string16& font,
                                    UScriptCode script) {
  settings->setPictographFontFamily(font, script);
}

void ApplyFontsFromMap(const ScriptFontFamilyMap& map,
                       SetFontFamilyWrapper setter,
                       blink::WebSettings* settings) {
  for (ScriptFontFamilyMap::const_iterator it = map.begin(); it != map.end();
       ++it) {
    int32 script = u_getPropertyValueEnum(UCHAR_SCRIPT, (it->first).c_str());
    if (script >= 0 && script < USCRIPT_CODE_LIMIT) {
      UScriptCode code = static_cast<UScriptCode>(script);
      (*setter)(settings, it->second, GetScriptForWebSettings(code));
    }
  }
}

}  // namespace

void BlinkSettingsImpl::ApplySettings(blink::WebView* web_view,
                                      const WebPreferences& prefs) const {
  blink::WebSettings* settings = web_view->settings();

  ApplyFontsFromMap(prefs.standard_font_family_map,
                    SetStandardFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.fixed_font_family_map, SetFixedFontFamilyWrapper,
                    settings);
  ApplyFontsFromMap(prefs.serif_font_family_map, SetSerifFontFamilyWrapper,
                    settings);
  ApplyFontsFromMap(prefs.sans_serif_font_family_map,
                    SetSansSerifFontFamilyWrapper, settings);
  ApplyFontsFromMap(prefs.cursive_font_family_map, SetCursiveFontFamilyWrapper,
                    settings);
  ApplyFontsFromMap(prefs.fantasy_font_family_map, SetFantasyFontFamilyWrapper,
                    settings);
  ApplyFontsFromMap(prefs.pictograph_font_family_map,
                    SetPictographFontFamilyWrapper, settings);

  settings->setDefaultFontSize(prefs.default_font_size);
  settings->setDefaultFixedFontSize(prefs.default_fixed_font_size);
  settings->setMinimumFontSize(prefs.minimum_font_size);
  settings->setMinimumLogicalFontSize(prefs.minimum_logical_font_size);

  settings->setDefaultTextEncodingName(
      base::ASCIIToUTF16(prefs.default_encoding));
  settings->setJavaScriptEnabled(prefs.javascript_enabled);

  settings->setWebSecurityEnabled(prefs.web_security_enabled);
  settings->setJavaScriptCanOpenWindowsAutomatically(
      prefs.javascript_can_open_windows_automatically);
  settings->setLoadsImagesAutomatically(prefs.loads_images_automatically);
  settings->setImagesEnabled(prefs.images_enabled);
  settings->setPluginsEnabled(prefs.plugins_enabled);
  settings->setDOMPasteAllowed(prefs.dom_paste_enabled);
  settings->setUsesEncodingDetector(prefs.uses_universal_detector);
  settings->setTextAreasAreResizable(prefs.text_areas_are_resizable);
  settings->setAllowScriptsToCloseWindows(prefs.allow_scripts_to_close_windows);
  settings->setDownloadableBinaryFontsEnabled(prefs.remote_fonts_enabled);
  settings->setJavaScriptCanAccessClipboard(
      prefs.javascript_can_access_clipboard);
  blink::WebRuntimeFeatures::enableXSLT(prefs.xslt_enabled);
  blink::WebRuntimeFeatures::enableSlimmingPaintV2(
      prefs.slimming_paint_v2_enabled);
  settings->setXSSAuditorEnabled(prefs.xss_auditor_enabled);
  settings->setDNSPrefetchingEnabled(prefs.dns_prefetching_enabled);
  settings->setLocalStorageEnabled(prefs.local_storage_enabled);
  settings->setSyncXHRInDocumentsEnabled(prefs.sync_xhr_in_documents_enabled);
  blink::WebRuntimeFeatures::enableDatabase(prefs.databases_enabled);
  settings->setOfflineWebApplicationCacheEnabled(
      prefs.application_cache_enabled);
  settings->setCaretBrowsingEnabled(prefs.caret_browsing_enabled);
  settings->setHyperlinkAuditingEnabled(prefs.hyperlink_auditing_enabled);
  settings->setCookieEnabled(prefs.cookie_enabled);
  settings->setNavigateOnDragDrop(prefs.navigate_on_drag_drop);

  // By default, allow_universal_access_from_file_urls is set to false and thus
  // we mitigate attacks from local HTML files by not granting file:// URLs
  // universal access. Only test shell will enable this.
  settings->setAllowUniversalAccessFromFileURLs(
      prefs.allow_universal_access_from_file_urls);
  settings->setAllowFileAccessFromFileURLs(
      prefs.allow_file_access_from_file_urls);

  // Enable the web audio API if requested on the command line.
  settings->setWebAudioEnabled(prefs.webaudio_enabled);

  // Enable experimental WebGL support if requested on command line
  // and support is compiled in.
  settings->setExperimentalWebGLEnabled(experimental_webgl_enabled_);

  // Disable GL multisampling if requested on command line.
  settings->setOpenGLMultisamplingEnabled(prefs.gl_multisampling_enabled);

  // Enable WebGL errors to the JS console if requested.
  settings->setWebGLErrorsToConsoleEnabled(
      prefs.webgl_errors_to_console_enabled);

  // Uses the mock theme engine for scrollbars.
  settings->setMockScrollbarsEnabled(prefs.mock_scrollbars_enabled);

  // Enable gpu-accelerated 2d canvas if requested on the command line.
  settings->setAccelerated2dCanvasEnabled(prefs.accelerated_2d_canvas_enabled);

  settings->setMinimumAccelerated2dCanvasSize(
      prefs.minimum_accelerated_2d_canvas_size);

  // Disable antialiasing for 2d canvas if requested on the command line.
  settings->setAntialiased2dCanvasEnabled(
      !prefs.antialiased_2d_canvas_disabled);

  // Enabled antialiasing of clips for 2d canvas if requested on the command
  // line.
  settings->setAntialiasedClips2dCanvasEnabled(
      prefs.antialiased_clips_2d_canvas_enabled);

  // Set MSAA sample count for 2d canvas if requested on the command line (or
  // default value if not).
  settings->setAccelerated2dCanvasMSAASampleCount(
      prefs.accelerated_2d_canvas_msaa_sample_count);

  settings->setAsynchronousSpellCheckingEnabled(
      prefs.asynchronous_spell_checking_enabled);
  settings->setUnifiedTextCheckerEnabled(prefs.unified_textchecker_enabled);

  // Tabs to link is not part of the settings. WebCore calls
  // ChromeClient::tabsToLinks which is part of the glue code.
  web_view->setTabsToLinks(prefs.tabs_to_links);

  settings->setAllowDisplayOfInsecureContent(
      prefs.allow_displaying_insecure_content);
  settings->setAllowRunningOfInsecureContent(
      prefs.allow_running_insecure_content);
  settings->setDisableReadingFromCanvas(prefs.disable_reading_from_canvas);
  settings->setStrictMixedContentChecking(prefs.strict_mixed_content_checking);

  settings->setStrictlyBlockBlockableMixedContent(
      prefs.strictly_block_blockable_mixed_content);

  settings->setStrictMixedContentCheckingForPlugin(
      prefs.block_mixed_plugin_content);

  settings->setStrictPowerfulFeatureRestrictions(
      prefs.strict_powerful_feature_restrictions);
  settings->setPasswordEchoEnabled(prefs.password_echo_enabled);
  settings->setShouldPrintBackgrounds(prefs.should_print_backgrounds);
  settings->setShouldClearDocumentBackground(
      prefs.should_clear_document_background);
  settings->setEnableScrollAnimator(prefs.enable_scroll_animator);

  blink::WebRuntimeFeatures::enableTouch(prefs.touch_enabled);
  settings->setMaxTouchPoints(prefs.pointer_events_max_touch_points);
  settings->setAvailablePointerTypes(prefs.available_pointer_types);
  settings->setPrimaryPointerType(
      static_cast<blink::WebSettings::PointerType>(prefs.primary_pointer_type));
  settings->setAvailableHoverTypes(prefs.available_hover_types);
  settings->setPrimaryHoverType(
      static_cast<blink::WebSettings::HoverType>(prefs.primary_hover_type));
  settings->setDeviceSupportsTouch(prefs.device_supports_touch);
  settings->setDeviceSupportsMouse(prefs.device_supports_mouse);
  settings->setEnableTouchAdjustment(prefs.touch_adjustment_enabled);
  settings->setMultiTargetTapNotificationEnabled(
      switches::IsLinkDisambiguationPopupEnabled());

  blink::WebRuntimeFeatures::enableImageColorProfiles(
      prefs.image_color_profiles_enabled);
  settings->setShouldRespectImageOrientation(
      prefs.should_respect_image_orientation);

  settings->setUnsafePluginPastingEnabled(false);
  settings->setEditingBehavior(
      static_cast<blink::WebSettings::EditingBehavior>(prefs.editing_behavior));

  settings->setSupportsMultipleWindows(prefs.supports_multiple_windows);

  // TODO(bokan): Remove once Blink side is gone.
  settings->setInvertViewportScrollOrder(true);

  settings->setViewportEnabled(prefs.viewport_enabled);
  settings->setLoadWithOverviewMode(prefs.initialize_at_minimum_page_scale);
  settings->setViewportMetaEnabled(prefs.viewport_meta_enabled);
  settings->setMainFrameResizesAreOrientationChanges(
      prefs.main_frame_resizes_are_orientation_changes);

  settings->setSmartInsertDeleteEnabled(prefs.smart_insert_delete_enabled);

  settings->setSpatialNavigationEnabled(prefs.spatial_navigation_enabled);

  settings->setSelectionIncludesAltImageText(true);

  settings->setV8CacheOptions(
      static_cast<blink::WebSettings::V8CacheOptions>(prefs.v8_cache_options));

  settings->setImageAnimationPolicy(
      static_cast<blink::WebSettings::ImageAnimationPolicy>(
          prefs.animation_policy));

  // Needs to happen before setIgnoreVIewportTagScaleLimits below.
  web_view->setDefaultPageScaleLimits(prefs.default_minimum_page_scale_factor,
                                      prefs.default_maximum_page_scale_factor);

#if defined(OS_ANDROID)
  settings->setAllowCustomScrollbarInMainFrame(false);
  settings->setTextAutosizingEnabled(prefs.text_autosizing_enabled);
  settings->setAccessibilityFontScaleFactor(prefs.font_scale_factor);
  settings->setDeviceScaleAdjustment(prefs.device_scale_adjustment);
  settings->setFullscreenSupported(prefs.fullscreen_supported);
  web_view->setIgnoreViewportTagScaleLimits(prefs.force_enable_zoom);
  settings->setAutoZoomFocusedNodeToLegibleScale(true);
  settings->setDoubleTapToZoomEnabled(prefs.double_tap_to_zoom_enabled);
  settings->setMediaControlsOverlayPlayButtonEnabled(true);
  settings->setMediaPlaybackRequiresUserGesture(
      prefs.user_gesture_required_for_media_playback);
  settings->setDefaultVideoPosterURL(
      base::ASCIIToUTF16(prefs.default_video_poster_url.spec()));
  settings->setSupportDeprecatedTargetDensityDPI(
      prefs.support_deprecated_target_density_dpi);
  settings->setUseLegacyBackgroundSizeShorthandBehavior(
      prefs.use_legacy_background_size_shorthand_behavior);
  settings->setWideViewportQuirkEnabled(prefs.wide_viewport_quirk);
  settings->setUseWideViewport(prefs.use_wide_viewport);
  settings->setForceZeroLayoutHeight(prefs.force_zero_layout_height);
  settings->setViewportMetaLayoutSizeQuirk(
      prefs.viewport_meta_layout_size_quirk);
  settings->setViewportMetaMergeContentQuirk(
      prefs.viewport_meta_merge_content_quirk);
  settings->setViewportMetaNonUserScalableQuirk(
      prefs.viewport_meta_non_user_scalable_quirk);
  settings->setViewportMetaZeroValuesQuirk(
      prefs.viewport_meta_zero_values_quirk);
  settings->setClobberUserAgentInitialScaleQuirk(
      prefs.clobber_user_agent_initial_scale_quirk);
  settings->setIgnoreMainFrameOverflowHiddenQuirk(
      prefs.ignore_main_frame_overflow_hidden_quirk);
  settings->setReportScreenSizeInPhysicalPixelsQuirk(
      prefs.report_screen_size_in_physical_pixels_quirk);
  settings->setPreferHiddenVolumeControls(true);
  settings->setMainFrameClipsContent(!prefs.record_whole_document);
  settings->setShrinksViewportContentToFit(true);
  settings->setUseMobileViewportStyle(true);
  settings->setAutoplayExperimentMode(
      blink::WebString::fromUTF8(prefs.autoplay_experiment_mode));
#endif

  blink::WebNetworkStateNotifier::setOnLine(prefs.is_online);
  blink::WebNetworkStateNotifier::setWebConnection(
      NetConnectionTypeToWebConnectionType(prefs.net_info_connection_type),
      prefs.net_info_max_bandwidth_mbps);

  settings->setPinchOverlayScrollbarThickness(
      prefs.pinch_overlay_scrollbar_thickness);
  settings->setUseSolidColorScrollbars(prefs.use_solid_color_scrollbars);

  settings->setShowContextMenuOnMouseUp(prefs.context_menu_on_mouse_up);

#if defined(OS_MACOSX)
  settings->setDoubleTapToZoomEnabled(true);
  web_view->setMaximumLegibleScale(prefs.default_maximum_page_scale_factor);
#endif
}

#if defined(OS_LINUX)
// TODO(rjkroege): Some of this information needs to be plumbed out of
// mus into the html_viewer. Most of this information needs to be
// dynamically adjustable (e.g. if I move a mandoline PlatformWindow from
// a LCD panel to a CRT display on a multiple-monitor system, then mus
// should communicate this to the app as part of having the app rasterize
// itself. See http://crbug.com/540054
void BlinkSettingsImpl::ApplyFontRendereringSettings() const {
  blink::WebFontRendering::setHinting(SkPaint::kNormal_Hinting);
  blink::WebFontRendering::setAutoHint(false);
  blink::WebFontRendering::setUseBitmaps(false);
  blink::WebFontRendering::setLCDOrder(
      gfx::FontRenderParams::SubpixelRenderingToSkiaLCDOrder(
          gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE));
  blink::WebFontRendering::setLCDOrientation(
      gfx::FontRenderParams::SubpixelRenderingToSkiaLCDOrientation(
          gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE));
  blink::WebFontRendering::setAntiAlias(true);
  blink::WebFontRendering::setSubpixelRendering(false);
  blink::WebFontRendering::setSubpixelPositioning(false);
  blink::WebFontRendering::setDefaultFontSize(0);
}
#else
void BlinkSettingsImpl::ApplyFontRendereringSettings() const {}
#endif

BlinkSettingsImpl::BlinkSettingsImpl() : experimental_webgl_enabled_(false) {}

void BlinkSettingsImpl::Init() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  experimental_webgl_enabled_ = !command_line->HasSwitch(kDisableWebGLSwitch);
}

void BlinkSettingsImpl::ApplySettingsToWebView(blink::WebView* view) const {
  WebPreferences prefs;
  ApplySettings(view, prefs);
  ApplyFontRendereringSettings();
}

}  // namespace html_viewer
