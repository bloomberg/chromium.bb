// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_

namespace content {
class WebContents;
}

namespace web_app {

class InstallManager {
 public:
  // Returns true if a web app can be installed for a given |web_contents|.
  virtual bool CanInstallWebApp(const content::WebContents* web_contents) = 0;

  // Starts a web app installation process for a given |web_contents|.
  virtual void InstallWebApp(content::WebContents* web_contents,
                             bool force_shortcut_app) = 0;

  virtual ~InstallManager() = default;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_MANAGER_H_
