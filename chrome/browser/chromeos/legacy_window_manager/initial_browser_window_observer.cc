// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/legacy_window_manager/initial_browser_window_observer.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

namespace {

// Taken from the --initial_chrome_window_mapped_file flag in the chromeos-wm
// command line: http://goo.gl/uLwIL
const char kInitialWindowFile[] =
    "/var/run/state/windowmanager/initial-chrome-window-mapped";

void WriteInitialWindowFile() {
  if (file_util::WriteFile(FilePath(kInitialWindowFile), "", 0) == -1)
    LOG(ERROR) << "Failed to touch " << kInitialWindowFile;
}

}  // namespace

namespace chromeos {

InitialBrowserWindowObserver::InitialBrowserWindowObserver() {
  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_WINDOW_READY,
                 content::NotificationService::AllSources());
}

void InitialBrowserWindowObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  registrar_.RemoveAll();
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&WriteInitialWindowFile));
}

}  // namespace chromeos
