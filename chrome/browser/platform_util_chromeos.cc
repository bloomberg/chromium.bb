// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include <gtk/gtk.h>

#include "base/file_util.h"
#include "base/process_util.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/filebrowse_ui.h"
#include "chrome/browser/ui/webui/mediaplayer_ui.h"
#include "chrome/common/process_watcher.h"
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

  Profile* profile;
  profile = BrowserList::GetLastActive()->profile();

  FileBrowseUI::OpenPopup(profile,
                          dir.value(),
                          FileBrowseUI::kPopupWidth,
                          FileBrowseUI::kPopupHeight);
}

// TODO(estade): It would be nice to be able to select the file in the file
// manager, but that probably requires extending xdg-open. For now just
// show the folder.
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
  if (ext == ".jpg" ||
      ext == ".jpeg" ||
      ext == ".png" ||
      ext == ".gif" ||
      ext == ".txt" ||
      ext == ".html" ||
      ext == ".htm") {
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
  if (ext == ".avi" ||
      ext == ".wav" ||
      ext == ".mp4" ||
      ext == ".mp3" ||
      ext == ".mkv" ||
      ext == ".ogg") {
    MediaPlayer* mediaplayer = MediaPlayer::GetInstance();
    std::string url = "file://";
    url += full_path.value();
    GURL gurl(url);
    mediaplayer->EnqueueMediaURL(gurl, NULL);
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
