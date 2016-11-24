// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_TEST_PREFERENCES_H_
#define COMPONENTS_TEST_RUNNER_TEST_PREFERENCES_H_

#include "components/test_runner/test_runner_export.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebSettings.h"

namespace test_runner {

struct TEST_RUNNER_EXPORT TestPreferences {
    int default_font_size;
    int minimum_font_size;
    bool xss_auditor_enabled;
    bool allow_file_access_from_file_urls;
    bool allow_running_of_insecure_content;
    blink::WebString default_text_encoding_name;
    bool experimental_webgl_enabled;
    bool experimental_css_grid_layout_enabled;
    bool java_script_can_access_clipboard;
    bool java_script_can_open_windows_automatically;
    bool supports_multiple_windows;
    bool java_script_enabled;
    bool loads_images_automatically;
    bool plugins_enabled;
    bool allow_universal_access_from_file_urls;
    blink::WebSettings::EditingBehavior editing_behavior;
    bool tabs_to_links;
    bool hyperlink_auditing_enabled;
    bool caret_browsing_enabled;
    bool should_respect_image_orientation;
    bool asynchronous_spell_checking_enabled;
    bool web_security_enabled;
    bool disable_reading_from_canvas;
    bool strict_mixed_content_checking;
    bool strict_powerful_feature_restrictions;
    bool spatial_navigation_enabled;

    TestPreferences();
    void Reset();
};

}

#endif  // COMPONENTS_TEST_RUNNER_TEST_PREFERENCES_H_
