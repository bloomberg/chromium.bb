// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

using content::BrowserThread;

namespace {

const char kGmailComposeUrl[] =
    "https://mail.google.com/mail/?extsrc=mailto&url=";

void OpenURL(const std::string& url) {
  // TODO(beng): improve this to locate context from call stack.
  Browser* browser = chrome::FindOrCreateTabbedBrowser(
      ProfileManager::GetDefaultProfileOrOffTheRecord(),
      chrome::HOST_DESKTOP_TYPE_ASH);
  chrome::NavigateParams params(
      browser, GURL(url), content::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

}  // namespace

namespace platform_util {

void ShowItemInFolder(const FilePath& full_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_manager_util::ShowFileInFolder(full_path);
}

void OpenItem(const FilePath& full_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_manager_util::ViewItem(full_path);
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
