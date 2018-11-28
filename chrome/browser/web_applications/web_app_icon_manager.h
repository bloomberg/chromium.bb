// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/web_application_info.h"

class Profile;
struct WebApplicationInfo;

namespace web_app {

class FileUtilsWrapper;

// Exclusively used from the UI thread.
class WebAppIconManager {
 public:
  WebAppIconManager(Profile* profile, std::unique_ptr<FileUtilsWrapper> utils);
  ~WebAppIconManager();

  using WriteDataCallback = base::OnceCallback<void(bool success)>;

  void WriteData(AppId app_id,
                 std::unique_ptr<WebApplicationInfo> web_app_info,
                 WriteDataCallback callback);

 private:
  base::FilePath web_apps_directory_;
  std::unique_ptr<FileUtilsWrapper> utils_;

  DISALLOW_COPY_AND_ASSIGN(WebAppIconManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_
