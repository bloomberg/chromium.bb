// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/blink_settings.h"

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/network_change_notifier.h"
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
const char kEnableAccelerated2DCanvas[] = "enable-accelerated-2d-canvas";

// Map of ISO 15924 four-letter script code to font family.  For example,
// "Arab" to "My Arabic Font".
typedef std::map<std::string, base::string16> ScriptFontFamilyMap;

enum EditingBehavior {
  EDITING_BEHAVIOR_MAC,
  EDITING_BEHAVIOR_WIN,
  EDITING_BEHAVIOR_UNIX,
  EDITING_BEHAVIOR_ANDROID,
  EDITING_BEHAVIOR_LAST = EDITING_BEHAVIOR_ANDROID
};

// Cache options for V8. See V8CacheOptions.h for information on the options.
enum V8CacheOptions {
  V8_CACHE_OPTIONS_DEFAULT,
  V8_CACHE_OPTIONS_NONE,
  V8_CACHE_OPTIONS_PARSE,
  V8_CACHE_OPTIONS_CODE,
  V8_CACHE_OPTIONS_LAST = V8_CACHE_OPTIONS_CODE
};

// ImageAnimationPolicy is used for controlling image animation when
// image frame is rendered for animation. See
// third_party/WebKit/Source/platform/graphics/ImageAnimationPolicy.h for
// information on the options.
enum ImageAnimationPolicy {
  IMAGE_ANIMATION_POLICY_ALLOWED,
  IMAGE_ANIMATION_POLICY_ANIMATION_ONCE,
  IMAGE_ANIMATION_POLICY_NO_ANIMATION
};

// A struct for managing blink's settings.
struct WebPreferences {
  ScriptFontFamilyMap standard_font_family_map;
  ScriptFontFamilyMap fixed_font_family_map;
  ScriptFontFamilyMap serif_font_family_map;
  ScriptFontFamilyMap sans_serif_font_family_map;
  ScriptFontFamilyMap cursive_font_family_map;
  ScriptFontFamilyMap fantasy_font_family_map;
  ScriptFontFamilyMap pictograph_font_family_map;
  int default_font_size;
  int default_fixed_font_size;
  int minimum_font_size;
  int minimum_logical_font_size;
  std::string default_encoding;
  bool context_menu_on_mouse_up;
  bool javascript_enabled;
  bool web_security_enabled;
  bool javascript_can_open_windows_automatically;
  bool loads_images_automatically;
  bool images_enabled;
  bool plugins_enabled;
  bool dom_paste_enabled;
  bool shrinks_standalone_images_to_fit;
  bool uses_universal_detector;
  bool text_areas_are_resizable;
  bool allow_scripts_to_close_windows;
  bool remote_fonts_enabled;
  bool javascript_can_access_clipboard;
  bool xslt_enabled;
  bool xss_auditor_enabled;
  // We don't use dns_prefetching_enabled to disable DNS prefetching.  Instead,
  // we disable the feature at a lower layer so that we catch non-WebKit uses
  // of DNS prefetch as well.
  bool dns_prefetching_enabled;
  bool local_storage_enabled;
  bool databases_enabled;
  bool application_cache_enabled;
  bool tabs_to_links;
  bool caret_browsing_enabled;
  bool hyperlink_auditing_enabled;
  bool is_online;
  net::NetworkChangeNotifier::ConnectionType net_info_connection_type;
  double net_info_max_bandwidth_mbps;
  bool allow_universal_access_from_file_urls;
  bool allow_file_access_from_file_urls;
  bool webaudio_enabled;
  bool experimental_webgl_enabled;
  bool pepper_3d_enabled;
  bool flash_3d_enabled;
  bool flash_stage3d_enabled;
  bool flash_stage3d_baseline_enabled;
  bool gl_multisampling_enabled;
  bool privileged_webgl_extensions_enabled;
  bool webgl_errors_to_console_enabled;
  bool mock_scrollbars_enabled;
  bool asynchronous_spell_checking_enabled;
  bool unified_textchecker_enabled;
  bool accelerated_2d_canvas_enabled;
  int minimum_accelerated_2d_canvas_size;
  bool antialiased_2d_canvas_disabled;
  bool antialiased_clips_2d_canvas_enabled;
  int accelerated_2d_canvas_msaa_sample_count;
  bool accelerated_filters_enabled;
  bool deferred_filters_enabled;
  bool container_culling_enabled;
  bool allow_displaying_insecure_content;
  bool allow_running_insecure_content;
  // If true, taints all <canvas> elements, regardless of origin.
  bool disable_reading_from_canvas;
  // Strict mixed content checking disables both displaying and running insecure
  // mixed content, and disables embedder notifications that such content was
  // requested (thereby preventing user override).
  bool strict_mixed_content_checking;
  // Strict powerful feature restrictions block insecure usage of powerful
  // features (like geolocation) that we haven't yet disabled for the web at
  // large.
  bool strict_powerful_feature_restrictions;
  // Disallow user opt-in for blockable mixed content.
  bool strictly_block_blockable_mixed_content;
  bool block_mixed_plugin_content;
  bool password_echo_enabled;
  bool should_print_backgrounds;
  bool should_clear_document_background;
  bool enable_scroll_animator;
  bool css_variables_enabled;
  bool touch_enabled;
  // TODO(mustaq): Nuke when the new API is ready
  bool device_supports_touch;
  // TODO(mustaq): Nuke when the new API is ready
  bool device_supports_mouse;
  bool touch_adjustment_enabled;
  int pointer_events_max_touch_points;
  int available_pointer_types;
  ui::PointerType primary_pointer_type;
  int available_hover_types;
  ui::HoverType primary_hover_type;
  bool sync_xhr_in_documents_enabled;
  bool image_color_profiles_enabled;
  bool should_respect_image_orientation;
  int number_of_cpu_cores;
  EditingBehavior editing_behavior;
  bool supports_multiple_windows;
  bool viewport_enabled;
  bool viewport_meta_enabled;
  bool main_frame_resizes_are_orientation_changes;
  bool initialize_at_minimum_page_scale;
  bool smart_insert_delete_enabled;
  bool spatial_navigation_enabled;
  int pinch_overlay_scrollbar_thickness;
  bool use_solid_color_scrollbars;
  bool navigate_on_drag_drop;
  V8CacheOptions v8_cache_options;
  bool slimming_paint_v2_enabled;
  bool inert_visual_viewport;

  // This flags corresponds to a Page's Settings' setCookieEnabled state. It
  // only controls whether or not the "document.cookie" field is properly
  // connected to the backing store, for instance if you wanted to be able to
  // define custom getters and setters from within a unique security content
  // without raising a DOM security exception.
  bool cookie_enabled;

  // This flag indicates whether H/W accelerated video decode is enabled for
  // pepper plugins. Defaults to false.
  bool pepper_accelerated_video_decode_enabled;

  ImageAnimationPolicy animation_policy;

#if defined(OS_ANDROID)
  bool text_autosizing_enabled;
  float font_scale_factor;
  float device_scale_adjustment;
  bool force_enable_zoom;
  bool fullscreen_supported;
  bool double_tap_to_zoom_enabled;
  bool user_gesture_required_for_media_playback;
  GURL default_video_poster_url;
  bool support_deprecated_target_density_dpi;
  bool use_legacy_background_size_shorthand_behavior;
  bool wide_viewport_quirk;
  bool use_wide_viewport;
  bool force_zero_layout_height;
  bool viewport_meta_layout_size_quirk;
  bool viewport_meta_merge_content_quirk;
  bool viewport_meta_non_user_scalable_quirk;
  bool viewport_meta_zero_values_quirk;
  bool clobber_user_agent_initial_scale_quirk;
  bool ignore_main_frame_overflow_hidden_quirk;
  bool report_screen_size_in_physical_pixels_quirk;
  bool record_whole_document;
  std::string autoplay_experiment_mode;
#endif

  // Default (used if the page or UA doesn't override these) values for page
  // scale limits. These are set directly on the WebView so there's no analogue
  // in WebSettings.
  float default_minimum_page_scale_factor;
  float default_maximum_page_scale_factor;

  // We try to keep the default values the same as the default values in
  // chrome, except for the cases where it would require lots of extra work for
  // the embedder to use the same default value.
  WebPreferences();
  ~WebPreferences();
};

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

// "Zyyy" is the ISO 15924 script code for undetermined script aka Common.
const char kCommonScript[] = "Zyyy";

WebPreferences::WebPreferences()
    : default_font_size(16),
      default_fixed_font_size(13),
      minimum_font_size(0),
      minimum_logical_font_size(6),
      default_encoding("ISO-8859-1"),
#if defined(OS_WIN)
      context_menu_on_mouse_up(true),
#else
      context_menu_on_mouse_up(false),
#endif
      javascript_enabled(true),
      web_security_enabled(true),
      javascript_can_open_windows_automatically(true),
      loads_images_automatically(true),
      images_enabled(true),
      // Content sets to true. False for mandoline.
      plugins_enabled(false),
      dom_paste_enabled(false),  // enables execCommand("paste")
      shrinks_standalone_images_to_fit(true),
      uses_universal_detector(false),  // Disabled: page cycler regression
      text_areas_are_resizable(true),
      allow_scripts_to_close_windows(false),
      remote_fonts_enabled(true),
      javascript_can_access_clipboard(false),
      xslt_enabled(true),
      xss_auditor_enabled(true),
      dns_prefetching_enabled(true),
      local_storage_enabled(false),
      databases_enabled(false),
      application_cache_enabled(false),
      tabs_to_links(true),
      caret_browsing_enabled(false),
      hyperlink_auditing_enabled(true),
      is_online(true),
      net_info_connection_type(net::NetworkChangeNotifier::CONNECTION_NONE),
      net_info_max_bandwidth_mbps(
          net::NetworkChangeNotifier::GetMaxBandwidthForConnectionSubtype(
              net::NetworkChangeNotifier::SUBTYPE_NONE)),
      allow_universal_access_from_file_urls(false),
      allow_file_access_from_file_urls(false),
      webaudio_enabled(false),
      experimental_webgl_enabled(false),
      pepper_3d_enabled(false),
      // Mandoline turns off flash.
      flash_3d_enabled(false),
      flash_stage3d_enabled(false),
      flash_stage3d_baseline_enabled(false),
      gl_multisampling_enabled(true),
      privileged_webgl_extensions_enabled(false),
      webgl_errors_to_console_enabled(true),
      mock_scrollbars_enabled(false),
      asynchronous_spell_checking_enabled(false),
      unified_textchecker_enabled(false),
      accelerated_2d_canvas_enabled(false),
      minimum_accelerated_2d_canvas_size(257 * 256),
      antialiased_2d_canvas_disabled(false),
      antialiased_clips_2d_canvas_enabled(false),
      accelerated_2d_canvas_msaa_sample_count(0),
      accelerated_filters_enabled(false),
      deferred_filters_enabled(false),
      container_culling_enabled(false),
      allow_displaying_insecure_content(true),
      allow_running_insecure_content(false),
      disable_reading_from_canvas(false),
      strict_mixed_content_checking(false),
      strict_powerful_feature_restrictions(false),
      strictly_block_blockable_mixed_content(false),
      block_mixed_plugin_content(false),
      password_echo_enabled(false),
      should_print_backgrounds(false),
      should_clear_document_background(true),
      enable_scroll_animator(false),
      touch_enabled(true),
      device_supports_touch(false),
      device_supports_mouse(true),
      // TODO(rjkroege): This is mandatory for mandoline page cyclers.
      // http://crbug.com:542885
      touch_adjustment_enabled(true),
      pointer_events_max_touch_points(11),
      available_pointer_types(0),
      primary_pointer_type(ui::POINTER_TYPE_NONE),
      available_hover_types(0),
      primary_hover_type(ui::HOVER_TYPE_NONE),
      sync_xhr_in_documents_enabled(true),
      image_color_profiles_enabled(false),
      should_respect_image_orientation(false),
      number_of_cpu_cores(1),
#if defined(OS_MACOSX)
      editing_behavior(EDITING_BEHAVIOR_MAC),
#elif defined(OS_WIN)
      editing_behavior(EDITING_BEHAVIOR_WIN),
#elif defined(OS_ANDROID)
      editing_behavior(EDITING_BEHAVIOR_ANDROID),
#elif defined(OS_POSIX)
      editing_behavior(EDITING_BEHAVIOR_UNIX),
#else
      editing_behavior(EDITING_BEHAVIOR_MAC),
#endif
      supports_multiple_windows(true),
      viewport_enabled(false),
#if defined(OS_ANDROID)
      viewport_meta_enabled(true),
#else
      viewport_meta_enabled(false),
#endif
      main_frame_resizes_are_orientation_changes(false),
      initialize_at_minimum_page_scale(true),
#if defined(OS_MACOSX)
      smart_insert_delete_enabled(true),
#else
      smart_insert_delete_enabled(false),
#endif
      spatial_navigation_enabled(false),
      pinch_overlay_scrollbar_thickness(0),
      use_solid_color_scrollbars(false),
      navigate_on_drag_drop(true),
      v8_cache_options(V8_CACHE_OPTIONS_DEFAULT),
      slimming_paint_v2_enabled(false),
      inert_visual_viewport(false),
      cookie_enabled(true),
      pepper_accelerated_video_decode_enabled(false),
      animation_policy(IMAGE_ANIMATION_POLICY_ALLOWED),
#if defined(OS_ANDROID)
      text_autosizing_enabled(true),
      font_scale_factor(1.0f),
      device_scale_adjustment(1.0f),
      force_enable_zoom(false),
      fullscreen_supported(true),
      double_tap_to_zoom_enabled(true),
      user_gesture_required_for_media_playback(true),
      support_deprecated_target_density_dpi(false),
      use_legacy_background_size_shorthand_behavior(false),
      wide_viewport_quirk(false),
      use_wide_viewport(true),
      force_zero_layout_height(false),
      viewport_meta_layout_size_quirk(false),
      viewport_meta_merge_content_quirk(false),
      viewport_meta_non_user_scalable_quirk(false),
      viewport_meta_zero_values_quirk(false),
      clobber_user_agent_initial_scale_quirk(false),
      ignore_main_frame_overflow_hidden_quirk(false),
      report_screen_size_in_physical_pixels_quirk(false),
      record_whole_document(false),
#endif
#if defined(OS_ANDROID)
      default_minimum_page_scale_factor(0.25f),
      default_maximum_page_scale_factor(5.f)
#elif defined(OS_MACOSX)
      default_minimum_page_scale_factor(1.f),
      default_maximum_page_scale_factor(3.f)
#else
      default_minimum_page_scale_factor(1.f),
      default_maximum_page_scale_factor(4.f)
#endif
{
  standard_font_family_map[kCommonScript] =
      base::ASCIIToUTF16("Times New Roman");
  fixed_font_family_map[kCommonScript] = base::ASCIIToUTF16("Courier New");
  serif_font_family_map[kCommonScript] = base::ASCIIToUTF16("Times New Roman");
  sans_serif_font_family_map[kCommonScript] = base::ASCIIToUTF16("Arial");
  cursive_font_family_map[kCommonScript] = base::ASCIIToUTF16("Script");
  fantasy_font_family_map[kCommonScript] = base::ASCIIToUTF16("Impact");
  pictograph_font_family_map[kCommonScript] =
      base::ASCIIToUTF16("Times New Roman");
}

WebPreferences::~WebPreferences() {}

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

void UpdateWebPreferencesForTest(WebPreferences* prefs) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  prefs->allow_universal_access_from_file_urls = true;
  prefs->dom_paste_enabled = true;
  prefs->javascript_can_access_clipboard = true;
  prefs->xslt_enabled = true;
  prefs->xss_auditor_enabled = false;
#if defined(OS_MACOSX)
  prefs->editing_behavior = EDITING_BEHAVIOR_MAC;
#else
  prefs->editing_behavior = EDITING_BEHAVIOR_WIN;
#endif
  prefs->application_cache_enabled = true;
  prefs->tabs_to_links = false;
  prefs->hyperlink_auditing_enabled = false;
  prefs->allow_displaying_insecure_content = true;
  prefs->allow_running_insecure_content = false;
  prefs->disable_reading_from_canvas = false;
  prefs->strict_mixed_content_checking = false;
  prefs->strict_powerful_feature_restrictions = false;
  prefs->webgl_errors_to_console_enabled = false;
  base::string16 serif;
#if defined(OS_MACOSX)
  prefs->cursive_font_family_map[kCommonScript] =
      base::ASCIIToUTF16("Apple Chancery");
  prefs->fantasy_font_family_map[kCommonScript] = base::ASCIIToUTF16("Papyrus");
  serif = base::ASCIIToUTF16("Times");
#else
  prefs->cursive_font_family_map[kCommonScript] =
      base::ASCIIToUTF16("Comic Sans MS");
  prefs->fantasy_font_family_map[kCommonScript] = base::ASCIIToUTF16("Impact");
  serif = base::ASCIIToUTF16("times new roman");
#endif
  prefs->serif_font_family_map[kCommonScript] = serif;
  prefs->standard_font_family_map[kCommonScript] = serif;
  prefs->fixed_font_family_map[kCommonScript] = base::ASCIIToUTF16("Courier");
  prefs->sans_serif_font_family_map[kCommonScript] =
      base::ASCIIToUTF16("Helvetica");
  prefs->minimum_logical_font_size = 9;
  prefs->asynchronous_spell_checking_enabled = false;
  prefs->accelerated_2d_canvas_enabled = false;
  command_line.HasSwitch(kEnableAccelerated2DCanvas);
  prefs->mock_scrollbars_enabled = false;
  prefs->smart_insert_delete_enabled = true;
  prefs->minimum_accelerated_2d_canvas_size = 0;
#if defined(OS_ANDROID)
  prefs->text_autosizing_enabled = false;
#endif
  prefs->viewport_enabled = false;
  prefs->default_minimum_page_scale_factor = 1.f;
  prefs->default_maximum_page_scale_factor = 4.f;
}

void ApplySettings(blink::WebView* web_view, bool test_mode) {
  WebPreferences prefs;

  if (test_mode) {
    UpdateWebPreferencesForTest(&prefs);
  }

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
  settings->setExperimentalWebGLEnabled(prefs.experimental_webgl_enabled);

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

  settings->setInertVisualViewport(prefs.inert_visual_viewport);

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
void ApplyFontRendereringSettings() {
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
void ApplyFontRendereringSettings() {}
#endif

}  // namespace

BlinkSettings::BlinkSettings()
    : layout_test_settings_(false), experimental_webgl_enabled_(false) {}

BlinkSettings::~BlinkSettings() {}

void BlinkSettings::Init() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  experimental_webgl_enabled_ = !command_line->HasSwitch(kDisableWebGLSwitch);
}

void BlinkSettings::SetLayoutTestMode() {
  layout_test_settings_ = true;
}

void BlinkSettings::ApplySettingsToWebView(blink::WebView* view) const {
  if (layout_test_settings_)
    ApplySettings(view, true /* test_mode */);
  else
    ApplySettings(view, false /* test_mode */);
  ApplyFontRendereringSettings();
}

}  // namespace html_viewer
