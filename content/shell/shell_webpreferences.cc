// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_webpreferences.h"

#include "webkit/glue/webpreferences.h"

namespace content {

ShellWebPreferences::ShellWebPreferences()
    : allow_universal_access_from_file_urls(true),
      dom_paste_enabled(true),
      javascript_can_access_clipboard(true),
      xss_auditor_enabled(true) {
}

ShellWebPreferences::~ShellWebPreferences() {}

void ShellWebPreferences::Apply(webkit_glue::WebPreferences* prefs) const {
  prefs->allow_universal_access_from_file_urls =
      allow_universal_access_from_file_urls;
  prefs->dom_paste_enabled = dom_paste_enabled;
  prefs->javascript_can_access_clipboard = javascript_can_access_clipboard;
  prefs->xss_auditor_enabled = xss_auditor_enabled;
#if !defined(OS_MACOSX)
  prefs->editing_behavior = webkit_glue::WebPreferences::EDITING_BEHAVIOR_WIN;
#endif
}

}  // namespace content
