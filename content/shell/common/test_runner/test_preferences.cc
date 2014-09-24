// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/test_runner/test_preferences.h"

#include "build/build_config.h"

using blink::WebSettings;
using blink::WebString;

namespace content {

TestPreferences::TestPreferences() { Reset(); }

void TestPreferences::Reset() {
  default_font_size = 16;
  minimum_font_size = 0;
  dom_paste_allowed = true;
  xss_auditor_enabled = false;
  allow_display_of_insecure_content = true;
  allow_file_access_from_file_urls = true;
  allow_running_of_insecure_content = true;
  default_text_encoding_name = WebString::fromUTF8("ISO-8859-1");
  experimental_webgl_enabled = false;
  experimental_css_regions_enabled = true;
  experimental_css_grid_layout_enabled = true;
  java_enabled = false;
  java_script_can_access_clipboard = true;
  java_script_can_open_windows_automatically = true;
  supports_multiple_windows = true;
  java_script_enabled = true;
  loads_images_automatically = true;
  offline_web_application_cache_enabled = true;
  plugins_enabled = true;
  caret_browsing_enabled = false;

  // Allow those layout tests running as local files, i.e. under
  // LayoutTests/http/tests/local, to access http server.
  allow_universal_access_from_file_urls = true;

#if defined(OS_MACOSX)
  editing_behavior = WebSettings::EditingBehaviorMac;
#else
  editing_behavior = WebSettings::EditingBehaviorWin;
#endif

  tabs_to_links = false;
  hyperlink_auditing_enabled = false;
  should_respect_image_orientation = false;
  asynchronous_spell_checking_enabled = false;
  web_security_enabled = true;
}

}  // namespace content
