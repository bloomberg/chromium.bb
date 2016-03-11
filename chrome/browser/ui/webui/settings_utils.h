// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_UTILS_H_

#include "base/macros.h"

namespace content {
class WebContents;
}

namespace settings_utils {

// Invoke UI for network proxy settings.
void ShowNetworkProxySettings(content::WebContents* web_contents);

// Invoke UI for SSL certificates.
void ShowManageSSLCertificates(content::WebContents* web_contents);

}  // namespace settings_utils

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_UTILS_H_
