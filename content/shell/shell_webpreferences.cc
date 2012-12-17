// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_webpreferences.h"

#include "webkit/glue/webpreferences.h"

namespace content {

void ShellWebPreferences::Export(webkit_glue::WebPreferences* prefs) const {
  prefs->allow_universal_access_from_file_urls =
      allowUniversalAccessFromFileURLs;
  prefs->dom_paste_enabled = DOMPasteAllowed;
  prefs->javascript_can_access_clipboard = javaScriptCanAccessClipboard;
  prefs->xss_auditor_enabled = XSSAuditorEnabled;
  prefs->editing_behavior =
      static_cast<webkit_glue::WebPreferences::EditingBehavior>(
          editingBehavior);
}

}  // namespace content
