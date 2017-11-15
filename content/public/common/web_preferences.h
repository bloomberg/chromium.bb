// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_WEB_PREFERENCES_H_
#define CONTENT_PUBLIC_COMMON_WEB_PREFERENCES_H_

#include <map>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "ui/base/touch/touch_device.h"
#include "url/gurl.h"

namespace blink {
class WebView;
}

namespace content {

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

// ImageAnimationPolicy is used for controlling image animation
// when image frame is rendered for animation.
// See third_party/WebKit/Source/platform/graphics/ImageAnimationPolicy.h
// for information on the options.
enum ImageAnimationPolicy {
  IMAGE_ANIMATION_POLICY_ALLOWED,
  IMAGE_ANIMATION_POLICY_ANIMATION_ONCE,
  IMAGE_ANIMATION_POLICY_NO_ANIMATION
};

enum class ViewportStyle { DEFAULT, MOBILE, TELEVISION, LAST = TELEVISION };

// Controls when the progress bar reports itself as complete. See
// third_party/WebKit/Source/core/loader/ProgressTracker.cpp for most of its
// effects.
enum class ProgressBarCompletion {
  LOAD_EVENT,
  RESOURCES_BEFORE_DCL,
  DOM_CONTENT_LOADED,
  RESOURCES_BEFORE_DCL_AND_SAME_ORIGIN_IFRAMES,
  LAST = RESOURCES_BEFORE_DCL_AND_SAME_ORIGIN_IFRAMES
};

enum class SavePreviousDocumentResources {
  NEVER,
  UNTIL_ON_DOM_CONTENT_LOADED,
  UNTIL_ON_LOAD,
  LAST = UNTIL_ON_LOAD
};

// Defines the autoplay policy to be used. Should match the class in
// WebSettings.h.
enum class AutoplayPolicy {
  kNoUserGestureRequired,
  kUserGestureRequired,
  kUserGestureRequiredForCrossOrigin,
  kDocumentUserActivationRequired,
};

// The ISO 15924 script code for undetermined script aka Common. It's the
// default used on WebKit's side to get/set a font setting when no script is
// specified.
CONTENT_EXPORT extern const char kCommonScript[];

// A struct for managing blink's settings.
//
// Adding new values to this class probably involves updating
// blink::WebSettings, content/common/view_messages.h, browser/tab_contents/
// render_view_host_delegate_helper.cc, browser/profiles/profile.cc,
// and content/public/common/common_param_traits_macros.h
struct CONTENT_EXPORT WebPreferences {
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
  bool loads_images_automatically;
  bool images_enabled;
  bool plugins_enabled;
  bool dom_paste_enabled;
  bool shrinks_standalone_images_to_fit;
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
  // Preference to save data. When enabled, requests will contain the header
  // 'Save-Data: on'.
  bool data_saver_enabled;
  bool local_storage_enabled;
  bool databases_enabled;
  bool application_cache_enabled;
  bool tabs_to_links;
  bool history_entry_requires_user_gesture;
  bool hyperlink_auditing_enabled;
  bool allow_universal_access_from_file_urls;
  bool allow_file_access_from_file_urls;
  bool webgl1_enabled;
  bool webgl2_enabled;
  bool pepper_3d_enabled;
  bool flash_3d_enabled;
  bool flash_stage3d_enabled;
  bool flash_stage3d_baseline_enabled;
  bool privileged_webgl_extensions_enabled;
  bool webgl_errors_to_console_enabled;
  bool mock_scrollbars_enabled;
  bool hide_scrollbars;
  bool accelerated_2d_canvas_enabled;
  int minimum_accelerated_2d_canvas_size;
  bool antialiased_2d_canvas_disabled;
  bool antialiased_clips_2d_canvas_enabled;
  int accelerated_2d_canvas_msaa_sample_count;
  bool accelerated_filters_enabled;
  bool deferred_filters_enabled;
  bool container_culling_enabled;
  bool allow_running_insecure_content;
  // If true, taints all <canvas> elements, regardless of origin.
  bool disable_reading_from_canvas;
  // Strict mixed content checking disables both displaying and running insecure
  // mixed content, and disables embedder notifications that such content was
  // requested (thereby preventing user override).
  bool strict_mixed_content_checking;
  // Strict powerful feature restrictions block insecure usage of powerful
  // features (like device orientation) that we haven't yet disabled for the web
  // at large.
  bool strict_powerful_feature_restrictions;
  // TODO(jww): Remove when WebView no longer needs this exception.
  bool allow_geolocation_on_insecure_origins;
  // Disallow user opt-in for blockable mixed content.
  bool strictly_block_blockable_mixed_content;
  bool block_mixed_plugin_content;
  bool password_echo_enabled;
  bool should_print_backgrounds;
  bool should_clear_document_background;
  bool enable_scroll_animator;
  bool touch_event_feature_detection_enabled;
  bool touch_adjustment_enabled;
  int pointer_events_max_touch_points;
  int available_pointer_types;
  ui::PointerType primary_pointer_type;
  int available_hover_types;
  ui::HoverType primary_hover_type;
  bool barrel_button_for_drag_enabled = false;
  bool sync_xhr_in_documents_enabled;
  bool should_respect_image_orientation;
  int number_of_cpu_cores;
  EditingBehavior editing_behavior;
  bool supports_multiple_windows;
  bool viewport_enabled;
  bool viewport_meta_enabled;
  bool shrinks_viewport_contents_to_fit;
  ViewportStyle viewport_style;
  bool always_show_context_menu_on_touch;
  bool main_frame_resizes_are_orientation_changes;
  bool initialize_at_minimum_page_scale;
  bool smart_insert_delete_enabled;
  bool spatial_navigation_enabled;
  bool use_solid_color_scrollbars;
  bool navigate_on_drag_drop;
  V8CacheOptions v8_cache_options;
  bool record_whole_document;
  SavePreviousDocumentResources save_previous_document_resources;

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

  bool user_gesture_required_for_presentation;

  // Specifies the margin for WebVTT text tracks as a percentage of media
  // element height/width (for horizontal/vertical text respectively).
  // Cues will not be placed in this margin area.
  float text_track_margin_percentage;

  bool page_popups_suppressed;

#if defined(OS_ANDROID)
  bool text_autosizing_enabled;
  float font_scale_factor;
  float device_scale_adjustment;
  bool force_enable_zoom;
  bool fullscreen_supported;
  bool double_tap_to_zoom_enabled;
  std::string media_playback_gesture_whitelist_scope;
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
  // Used by Android_WebView only to support legacy apps that inject script into
  // a top-level initial empty document and expect it to persist on navigation.
  bool resue_global_for_unowned_main_frame;
  ProgressBarCompletion progress_bar_completion;
  // Specifies default setting for spellcheck when the spellcheck attribute is
  // not explicitly specified.
  bool spellcheck_enabled_by_default;
  // If enabled, when a video goes fullscreen, the orientation should be locked.
  bool video_fullscreen_orientation_lock_enabled;
  // If enabled, fullscreen should be entered/exited when the device is rotated
  // to/from the orientation of the video.
  bool video_rotate_to_fullscreen_enabled;
  // If enabled, video fullscreen detection will be enabled.
  bool video_fullscreen_detection_enabled;
  bool embedded_media_experience_enabled;
  // Enable 8 (#RRGGBBAA) and 4 (#RGBA) value hex colors in CSS Android
  // WebView quirk (http://crbug.com/618472).
  bool css_hex_alpha_color_enabled;
  bool enable_media_download_in_product_help;
  // Enable support for document.scrollingElement
  // WebView sets this to false to retain old documentElement behaviour
  // (http://crbug.com/761016).
  bool scroll_top_left_interop_enabled;
#else  // defined(OS_ANDROID)
#endif  // defined(OS_ANDROID)

  // Default (used if the page or UA doesn't override these) values for page
  // scale limits. These are set directly on the WebView so there's no analogue
  // in WebSettings.
  float default_minimum_page_scale_factor;
  float default_maximum_page_scale_factor;

  // Whether download UI should be hidden on this page.
  bool hide_download_ui;

  // If enabled, disabled video track when the video is in the background.
  bool background_video_track_optimization_enabled;

  // Whether it is a presentation receiver.
  bool presentation_receiver;

  // If disabled, media controls should never be used.
  bool media_controls_enabled;

  // Whether we want to disable updating selection on mutating selection range.
  // This is to work around Samsung's email app issue. See
  // https://crbug.com/699943 for details.
  // TODO(changwan): remove this once we no longer support Android N.
  bool do_not_update_selection_on_mutating_selection_range;

  // Defines the current autoplay policy.
  AutoplayPolicy autoplay_policy;

  // We try to keep the default values the same as the default values in
  // chrome, except for the cases where it would require lots of extra work for
  // the embedder to use the same default value.
  WebPreferences();
  WebPreferences(const WebPreferences& other);
  ~WebPreferences();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_WEB_PREFERENCES_H_
