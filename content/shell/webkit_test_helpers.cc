// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/webkit_test_helpers.h"

#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebPreferences.h"
#include "webkit/glue/webpreferences.h"

using WebTestRunner::WebPreferences;

namespace content {

void ExportPreferences(const WebPreferences& from,
                       webkit_glue::WebPreferences* to) {
  to->allow_universal_access_from_file_urls =
      from.allowUniversalAccessFromFileURLs;
  to->dom_paste_enabled = from.DOMPasteAllowed;
  to->javascript_can_access_clipboard = from.javaScriptCanAccessClipboard;
  to->xss_auditor_enabled = from.XSSAuditorEnabled;
  to->editing_behavior =
      static_cast<webkit_glue::WebPreferences::EditingBehavior>(
          from.editingBehavior);
}

}  // namespace content
