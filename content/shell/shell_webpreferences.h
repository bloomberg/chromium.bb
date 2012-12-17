// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_WEBPREFERENCES_H_
#define CONTENT_SHELL_SHELL_WEBPREFERENCES_H_

#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebPreferences.h"

namespace webkit_glue {
struct WebPreferences;
}

namespace content {

struct ShellWebPreferences : public WebTestRunner::WebPreferences {
  void Export(webkit_glue::WebPreferences* prefs) const;
};

}  // namespace content

#endif   // CONTENT_SHELL_SHELL_WEBPREFERENCES__H_
