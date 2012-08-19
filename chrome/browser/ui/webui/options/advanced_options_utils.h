// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_ADVANCED_OPTIONS_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_ADVANCED_OPTIONS_UTILS_H_

#include "base/basictypes.h"

namespace content {
class WebContents;
}

namespace options {

// Chrome advanced options utility methods.
class AdvancedOptionsUtilities {
 public:
  // Invoke UI for network proxy settings.
  static void ShowNetworkProxySettings(content::WebContents*);

  // Invoke UI for SSL certificates.
  static void ShowManageSSLCertificates(content::WebContents*);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AdvancedOptionsUtilities);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_ADVANCED_OPTIONS_UTILS_H_
