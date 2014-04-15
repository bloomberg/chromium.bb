// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_UPDATE_SHORTCUT_WORKER_WIN_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_UPDATE_SHORTCUT_WORKER_WIN_H_

#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class NotificationDetails;
class NotificationSource;
class WebContents;
}

namespace web_app {

// UpdateShortcutWorker holds all context data needed for update shortcut.
// It schedules a pre-update check to find all shortcuts that needs to be
// updated. If there are such shortcuts, it schedules icon download and
// update them when icons are downloaded. It observes TAB_CLOSING notification
// and cancels all the work when the underlying tab is closing.
class UpdateShortcutWorker : public content::NotificationObserver {
 public:
  explicit UpdateShortcutWorker(content::WebContents* web_contents);

  void Run();

 private:
  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  // Downloads icon via the FaviconTabHelper.
  void DownloadIcon();

  // Favicon download callback.
  void DidDownloadFavicon(
      int requested_size,
      int id,
      int http_status_code,
      const GURL& image_url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes);

  // Checks if shortcuts exists on desktop, start menu and quick launch.
  void CheckExistingShortcuts();

  // Update shortcut files and icons.
  void UpdateShortcuts();
  void UpdateShortcutsOnFileThread();

  // Callback after shortcuts are updated.
  void OnShortcutsUpdated(bool);

  // Deletes the worker on UI thread where it gets created.
  void DeleteMe();
  void DeleteMeOnUIThread();

  content::NotificationRegistrar registrar_;

  // Underlying WebContents whose shortcuts will be updated.
  content::WebContents* web_contents_;

  // Icons info from web_contents_'s web app data.
  web_app::IconInfoList unprocessed_icons_;

  // Cached shortcut data from the web_contents_.
  web_app::ShortcutInfo shortcut_info_;

  // Our copy of profile path.
  base::FilePath profile_path_;

  // File name of shortcut/ico file based on app title.
  base::FilePath file_name_;

  // Existing shortcuts.
  std::vector<base::FilePath> shortcut_files_;

  DISALLOW_COPY_AND_ASSIGN(UpdateShortcutWorker);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_UPDATE_SHORTCUT_WORKER_WIN_H_
