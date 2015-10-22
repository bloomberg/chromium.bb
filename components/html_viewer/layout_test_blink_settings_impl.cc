// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/layout_test_blink_settings_impl.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "components/html_viewer/web_preferences.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"

#if defined(OS_WIN)
#include "ui/gfx/win/dpi.h"
#endif

namespace html_viewer {

namespace {

const char kEnableAccelerated2DCanvas[] = "enable-accelerated-2d-canvas";

void UpdateWebPreferencesForTest(WebPreferences* prefs,
                                 bool accelerated_2d_canvas_enabled) {
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
  prefs->accelerated_2d_canvas_enabled = accelerated_2d_canvas_enabled;
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

}  // namespace

LayoutTestBlinkSettingsImpl::LayoutTestBlinkSettingsImpl()
    : BlinkSettingsImpl(), accelerated_2d_canvas_enabled_(false) {}

void LayoutTestBlinkSettingsImpl::Init() {
  BlinkSettingsImpl::Init();
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  accelerated_2d_canvas_enabled_ =
      command_line.HasSwitch(kEnableAccelerated2DCanvas);
}

void LayoutTestBlinkSettingsImpl::ApplySettingsToWebView(
    blink::WebView* view) const {
  WebPreferences prefs;
  UpdateWebPreferencesForTest(&prefs, accelerated_2d_canvas_enabled_);
  ApplySettings(view, prefs);
  ApplyFontRendereringSettings();

  blink::WebRuntimeFeatures::enableExperimentalFeatures(true);
  blink::WebRuntimeFeatures::enableTestOnlyFeatures(true);

#if defined(OS_WIN)
  gfx::InitDeviceScaleFactor(1.0f);
#endif
}

}  // namespace html_viewer
