// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include <gtk/gtk.h>

#include "base/file_util.h"
#include "base/process_util.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/file_manager_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/mediaplayer_ui.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "content/browser/browser_thread.h"
#include "content/common/process_watcher.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

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
  std::string ext = full_path.Extension();
  // For things supported natively by the browser, we should open it
  // in a tab.
  if (base::strcasecmp(ext.data(), ".jpg") == 0 ||
      base::strcasecmp(ext.data(), ".jpeg") == 0 ||
      base::strcasecmp(ext.data(), ".png") == 0 ||
      base::strcasecmp(ext.data(), ".gif") == 0 ||
      base::strcasecmp(ext.data(), ".txt") == 0 ||
      base::strcasecmp(ext.data(), ".html") == 0 ||
      base::strcasecmp(ext.data(), ".htm") == 0) {
    std::string path;
    path = "file://";
    path.append(full_path.value());
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      bool result = BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableFunction(&OpenItem, full_path));
      DCHECK(result);
      return;
    }
    Browser* browser = BrowserList::GetLastActive();
    browser->AddSelectedTabWithURL(GURL(path), PageTransition::LINK);
    return;
  }
  if (base::strcasecmp(ext.data(), ".avi") == 0 ||
      base::strcasecmp(ext.data(), ".wav") == 0 ||
      base::strcasecmp(ext.data(), ".mp4") == 0 ||
      base::strcasecmp(ext.data(), ".mp3") == 0 ||
      base::strcasecmp(ext.data(), ".mkv") == 0 ||
      base::strcasecmp(ext.data(), ".ogg") == 0) {
    Browser* browser = BrowserList::GetLastActive();
    if (!browser)
      return;
    MediaPlayer* mediaplayer = MediaPlayer::GetInstance();
    mediaplayer->EnqueueMediaFile(browser->profile(), full_path, NULL);
    return;
  }

  // Unknwon file type. Show an error message to user.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(
          &SimpleErrorBox,
          static_cast<gfx::NativeWindow>(NULL),
          l10n_util::GetStringUTF16(IDS_FILEBROWSER_ERROR_TITLE),
          l10n_util::GetStringFUTF16(IDS_FILEBROWSER_ERROR_UNKNOWN_FILE_TYPE,
                                     UTF8ToUTF16(full_path.BaseName().value()))
          ));
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
