// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_SHORTCUT_INSTALLATION_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_SHORTCUT_INSTALLATION_TASK_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_installation_task.h"
#include "chrome/common/web_application_info.h"

namespace content {
class WebContents;
}

namespace extensions {

// Subclass of BookmarkAppInstallationTask that exclusively installs
// BookmarkApp-based Shortcuts.
class BookmarkAppShortcutInstallationTask : public BookmarkAppInstallationTask {
 public:
  BookmarkAppShortcutInstallationTask();
  ~BookmarkAppShortcutInstallationTask() override;

  void InstallFromWebContents(content::WebContents* web_contents,
                              ResultCallback callback);

 private:
  void OnGetWebApplicationInfo(
      ResultCallback result_callback,
      std::unique_ptr<WebApplicationInfo> web_app_info);
  void OnGetIcons(ResultCallback result_callback,
                  std::vector<WebApplicationInfo::IconInfo> icons);

  base::WeakPtrFactory<BookmarkAppShortcutInstallationTask> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppShortcutInstallationTask);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_SHORTCUT_INSTALLATION_TASK_H_
