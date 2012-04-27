// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;

class Profile;

namespace {

const char kGmailComposeUrl[] =
    "https://mail.google.com/mail/?extsrc=mailto&url=";

void OpenItemOnFileThread(const FilePath& full_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::Closure callback;
  if (file_util::DirectoryExists(full_path))
    callback = base::Bind(&file_manager_util::ViewFolder, full_path);
  else
    callback = base::Bind(&file_manager_util::ViewFile, full_path);
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
  file_manager_util::ShowFileInFolder(full_path);
}

void OpenItem(const FilePath& full_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&OpenItemOnFileThread, full_path));
}

void OpenExternal(const GURL& url) {
  // This code should be obsolete since we have default handlers in ChromeOS
  // which should handle this. However - there are two things which make it
  // necessary to keep it in:
  // a.) The user might have deleted the default handler in this session.
  //     In this case we would need to have this in place.
  // b.) There are several code paths which are not clear if they would call
  //     this function directly and which would therefore break (e.g.
  //     "Browser::EmailPageLocation" (to name only one).
  // As such we should keep this code here.
  if (url.SchemeIs("mailto")) {
    std::string string_url = kGmailComposeUrl;
    string_url.append(url.spec());
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(OpenURL, string_url));
  }
}

}  // namespace platform_util
