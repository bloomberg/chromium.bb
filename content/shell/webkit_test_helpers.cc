// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/webkit_test_helpers.h"

#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebPreferences.h"
#include "webkit/glue/webpreferences.h"

using WebTestRunner::WebPreferences;

namespace content {

void ExportLayoutTestSpecificPreferences(const WebPreferences& from,
                                         webkit_glue::WebPreferences* to) {
  to->allow_universal_access_from_file_urls =
      from.allowUniversalAccessFromFileURLs;
  to->dom_paste_enabled = from.DOMPasteAllowed;
  to->javascript_can_access_clipboard = from.javaScriptCanAccessClipboard;
  to->xss_auditor_enabled = from.XSSAuditorEnabled;
  to->editing_behavior =
      static_cast<webkit_glue::WebPreferences::EditingBehavior>(
          from.editingBehavior);
  to->default_font_size = from.defaultFontSize;
  to->minimum_font_size = from.minimumFontSize;
  to->default_encoding = from.defaultTextEncodingName.utf8().data();
  to->javascript_enabled = from.javaScriptEnabled;
  to->supports_multiple_windows = from.supportsMultipleWindows;
  to->loads_images_automatically = from.loadsImagesAutomatically;
  to->plugins_enabled = from.pluginsEnabled;
  to->java_enabled = from.javaEnabled;
  to->uses_page_cache = from.usesPageCache;
  to->page_cache_supports_plugins = from.pageCacheSupportsPlugins;
  to->application_cache_enabled = from.offlineWebApplicationCacheEnabled;
  to->tabs_to_links = from.tabsToLinks;
  to->experimental_webgl_enabled = from.experimentalWebGLEnabled;
  to->css_grid_layout_enabled = from.experimentalCSSGridLayoutEnabled;
  // experimentalCSSRegionsEnabled is deprecated and ignored.
  to->hyperlink_auditing_enabled = from.hyperlinkAuditingEnabled;
  to->caret_browsing_enabled = from.caretBrowsingEnabled;
  to->allow_displaying_insecure_content = from.allowDisplayOfInsecureContent;
  to->allow_running_insecure_content = from.allowRunningOfInsecureContent;
  to->css_shaders_enabled = from.cssCustomFilterEnabled;
  to->should_respect_image_orientation = from.shouldRespectImageOrientation;
}

void CopyLayoutTestSpecificPreferences(const webkit_glue::WebPreferences& from,
                                       webkit_glue::WebPreferences* to) {
  to->allow_universal_access_from_file_urls =
      from.allow_universal_access_from_file_urls;
  to->dom_paste_enabled = from.dom_paste_enabled;
  to->javascript_can_access_clipboard = from.javascript_can_access_clipboard;
  to->xss_auditor_enabled = from.xss_auditor_enabled;
  to->editing_behavior = from.editing_behavior;
  to->default_font_size = from.default_font_size;
  to->minimum_font_size = from.minimum_font_size;
  to->default_encoding = from.default_encoding;
  to->javascript_enabled = from.javascript_enabled;
  to->supports_multiple_windows = from.supports_multiple_windows;
  to->loads_images_automatically = from.loads_images_automatically;
  to->plugins_enabled = from.plugins_enabled;
  to->java_enabled = from.java_enabled;
  to->uses_page_cache = from.uses_page_cache;
  to->page_cache_supports_plugins = from.page_cache_supports_plugins;
  to->application_cache_enabled = from.application_cache_enabled;
  to->tabs_to_links = from.tabs_to_links;
  to->experimental_webgl_enabled = from.experimental_webgl_enabled;
  to->css_grid_layout_enabled = from.css_grid_layout_enabled;
  to->hyperlink_auditing_enabled = from.hyperlink_auditing_enabled;
  to->caret_browsing_enabled = from.caret_browsing_enabled;
  to->allow_displaying_insecure_content =
      from.allow_displaying_insecure_content;
  to->allow_running_insecure_content = from.allow_running_insecure_content;
  to->css_shaders_enabled = from.css_shaders_enabled;
  to->should_respect_image_orientation = from.should_respect_image_orientation;
}

}  // namespace content
