// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include "base/file_util.h"
#include "base/process_util.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/file_manager_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "content/browser/browser_thread.h"
#include "content/common/process_watcher.h"
#include "googleurl/src/gurl.h"

class Profile;

namespace platform_util {

static const std::string kGmailComposeUrl =
    "https://mail.google.com/mail/?extsrc=mailto&url=";

// Opens file browser on UI thread.
void OpenFileBrowserOnUIThread(const FilePath& dir) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return;
  GURL url = FileManagerUtil::GetFileBrowserUrlWithParams(
     SelectFileDialog::SELECT_NONE, string16(), dir, NULL, 0,
     FilePath::StringType());
  browser->ShowSingletonTab(url);
}

void ShowItemInFolder(const FilePath& full_path) {
  FilePath dir = full_path.DirName();
  if (!file_util::DirectoryExists(dir))
    return;

  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    OpenFileBrowserOnUIThread(dir);
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableFunction(&OpenFileBrowserOnUIThread, dir));
  }
}

void OpenItem(const FilePath& full_path) {
  FileManagerUtil::ViewItem(full_path, false);
}

static void OpenURL(const std::string& url) {
  Browser* browser = BrowserList::GetLastActive();
  browser->AddSelectedTabWithURL(GURL(url), PageTransition::LINK);
}

void OpenExternal(const GURL& url) {
  if (url.SchemeIs("mailto")) {
    std::string string_url = kGmailComposeUrl;
    string_url.append(url.spec());
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, NewRunnableFunction(OpenURL, string_url));
  }
}

}  // namespace platform_util
