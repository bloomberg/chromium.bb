// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_WEBPREFERENCES_H_
#define CONTENT_SHELL_SHELL_WEBPREFERENCES_H_

namespace webkit_glue {
struct WebPreferences;
}

namespace content {

struct ShellWebPreferences {
  bool allow_universal_access_from_file_urls;
  bool dom_paste_enabled;
  bool javascript_can_access_clipboard;
  bool xss_auditor_enabled;

  ShellWebPreferences();
  ~ShellWebPreferences();

  void Apply(webkit_glue::WebPreferences* prefs) const;
};

}  // namespace content

#endif   // CONTENT_SHELL_SHELL_WEBPREFERENCES__H_
