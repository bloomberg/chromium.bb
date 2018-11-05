// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_

struct WebApplicationInfo;

namespace blink {
struct Manifest;
}

namespace web_app {

enum class ForInstallableSite {
  kYes,
  kNo,
  kUnknown,
};

// Update the given WebApplicationInfo with information from the manifest.
void UpdateWebAppInfoFromManifest(const blink::Manifest& manifest,
                                  WebApplicationInfo* web_app_info,
                                  ForInstallableSite installable_site);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_
