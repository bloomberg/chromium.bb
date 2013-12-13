// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include "chrome/browser/chromeos/file_manager/open_util.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

const char kGmailComposeUrl[] =
    "https://mail.google.com/mail/?extsrc=mailto&url=";

}  // namespace

namespace platform_util {

void ShowItemInFolder(Profile* profile, const base::FilePath& full_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_manager::util::ShowItemInFolder(profile, full_path);
}

void OpenItem(Profile* profile, const base::FilePath& full_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_manager::util::OpenItem(profile, full_path);
}

void OpenExternal(Profile* profile, const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // This code should be obsolete since we have default handlers in ChromeOS
  // which should handle this. However - there are two things which make it
  // necessary to keep it in:
  // a.) The user might have deleted the default handler in this session.
  //     In this case we would need to have this in place.
  // b.) There are several code paths which are not clear if they would call
  //     this function directly and which would therefore break (e.g.
  //     "Browser::EmailPageLocation" (to name only one).
  // As such we should keep this code here.
  chrome::NavigateParams params(profile, url, content::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  params.host_desktop_type = chrome::HOST_DESKTOP_TYPE_ASH;

  if (url.SchemeIs("mailto")) {
    std::string string_url = kGmailComposeUrl;
    string_url.append(url.spec());
    params.url = GURL(url);
  }

  chrome::Navigate(&params);
}

}  // namespace platform_util
