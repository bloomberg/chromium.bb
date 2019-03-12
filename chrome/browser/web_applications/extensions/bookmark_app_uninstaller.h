// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_UNINSTALLER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_UNINSTALLER_H_

#include "chrome/browser/web_applications/extensions/web_app_extension_ids_map.h"

class Profile;
class GURL;

namespace web_app {
class AppRegistrar;
}

namespace extensions {

class BookmarkAppUninstaller {
 public:
  BookmarkAppUninstaller(Profile* profile, web_app::AppRegistrar* registrar);
  ~BookmarkAppUninstaller();

  // Returns true if the app with |app_url| was successfully uninstalled.
  // Returns false if the app doesn't not exist, or the app failed to be
  // uninstalled.
  bool UninstallApp(const GURL& app_url);

 private:
  Profile* profile_;
  web_app::AppRegistrar* registrar_;
  web_app::ExtensionIdsMap extension_ids_map_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_UNINSTALLER_H_
