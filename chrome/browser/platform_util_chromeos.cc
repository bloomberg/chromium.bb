// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/process_util.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/common/process_watcher.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;

class Profile;

namespace {

const char kGmailComposeUrl[] =
    "https://mail.google.com/mail/?extsrc=mailto&url=";

void OpenFileBrowserOnUIThread(const FilePath& dir) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    return;

  FilePath virtual_path;
  if (!file_manager_util::ConvertFileToRelativeFileSystemPath(
      browser->profile(), dir, &virtual_path)) {
    return;
  }

  GURL url = file_manager_util::GetFileBrowserUrlWithParams(
     SelectFileDialog::SELECT_NONE, string16(), virtual_path, NULL, 0,
     FilePath::StringType());
  browser->ShowSingletonTab(url);
}

// file_util::DirectoryExists must be called on the FILE thread.
void ShowItemInFolderOnFileThread(const FilePath& full_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  FilePath dir = full_path.DirName();
  if (file_util::DirectoryExists(dir)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&OpenFileBrowserOnUIThread, dir));
  }
}

void OpenItemOnFileThread(const FilePath& full_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::Closure callback;
  if (file_util::DirectoryExists(full_path))
    callback = base::Bind(&file_manager_util::ViewFolder, full_path);
  else
    callback = base::Bind(&file_manager_util::ViewItem, full_path, false);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}

void OpenURL(const std::string& url) {
  Browser* browser = BrowserList::GetLastActive();
  browser->AddSelectedTabWithURL(GURL(url), content::PAGE_TRANSITION_LINK);
}

}  // namespace

namespace platform_util {

void ShowItemInFolder(const FilePath& full_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&ShowItemInFolderOnFileThread, full_path));
}

void OpenItem(const FilePath& full_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&OpenItemOnFileThread, full_path));
}

void OpenExternal(const GURL& url) {
  if (url.SchemeIs("mailto")) {
    std::string string_url = kGmailComposeUrl;
    string_url.append(url.spec());
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(OpenURL, string_url));
  }
}

}  // namespace platform_util
