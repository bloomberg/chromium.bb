// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_FILE_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_FILE_HANDLER_MANAGER_H_

#include <vector>

#include "chrome/browser/web_applications/components/file_handler_manager.h"

namespace web_app {
class WebAppMigrationManager;
}  // namespace web_app

namespace extensions {

class BookmarkAppFileHandlerManager : public web_app::FileHandlerManager {
 public:
  explicit BookmarkAppFileHandlerManager(Profile* profile);
  ~BookmarkAppFileHandlerManager() override;

 protected:
  const apps::FileHandlers* GetAllFileHandlers(
      const web_app::AppId& app_id) override;

  friend class web_app::WebAppMigrationManager;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_FILE_HANDLER_MANAGER_H_
