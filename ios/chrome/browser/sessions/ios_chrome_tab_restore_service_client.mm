// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_client.h"

#include "components/sessions/ios/ios_live_tab.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/sessions/tab_restore_service_delegate_impl_ios.h"
#include "ios/chrome/browser/sessions/tab_restore_service_delegate_impl_ios_factory.h"
#include "ios/chrome/browser/tabs/tab.h"
#include "ios/chrome/browser/tabs/tab_model.h"
#include "ios/chrome/browser/ui/browser_ios.h"
#include "ios/chrome/browser/ui/browser_list_ios.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/web/public/web_thread.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeTabRestoreServiceClient::IOSChromeTabRestoreServiceClient(
    ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {}

IOSChromeTabRestoreServiceClient::~IOSChromeTabRestoreServiceClient() {}

sessions::LiveTabContext*
IOSChromeTabRestoreServiceClient::CreateLiveTabContext(
    const std::string& app_name) {
  return TabRestoreServiceDelegateImplIOSFactory::GetForBrowserState(
      browser_state_);
}

sessions::LiveTabContext*
IOSChromeTabRestoreServiceClient::FindLiveTabContextForTab(
    const sessions::LiveTab* tab) {
  const sessions::IOSLiveTab* requested_tab =
      static_cast<const sessions::IOSLiveTab*>(tab);
  for (BrowserListIOS::const_iterator iter = BrowserListIOS::begin();
       iter != BrowserListIOS::end(); ++iter) {
    id<BrowserIOS> browser = *iter;
    for (Tab* current_tab in [browser tabModel]) {
      if (current_tab.webState &&
          current_tab.webState == requested_tab->web_state()) {
        return TabRestoreServiceDelegateImplIOSFactory::GetForBrowserState(
            [browser browserState]);
      }
    }
  }
  return nullptr;
}

sessions::LiveTabContext*
IOSChromeTabRestoreServiceClient::FindLiveTabContextWithID(
    SessionID::id_type desired_id) {
  for (BrowserListIOS::const_iterator iter = BrowserListIOS::begin();
       iter != BrowserListIOS::end(); ++iter) {
    id<BrowserIOS> browser = *iter;
    if ([browser tabModel].sessionID.id() == desired_id) {
      return TabRestoreServiceDelegateImplIOSFactory::GetForBrowserState(
          [browser browserState]);
    }
  }
  return nullptr;
}

bool IOSChromeTabRestoreServiceClient::ShouldTrackURLForRestore(
    const GURL& url) {
  // NOTE: In the //chrome client, chrome://quit and chrome://restart are
  // blacklisted from being tracked to avoid entering infinite loops. However,
  // iOS intentionally does not support those URLs, so there is no need to
  // blacklist them here.
  return url.is_valid();
}

std::string IOSChromeTabRestoreServiceClient::GetExtensionAppIDForTab(
    sessions::LiveTab* tab) {
  return std::string();
}

base::SequencedWorkerPool* IOSChromeTabRestoreServiceClient::GetBlockingPool() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return web::WebThread::GetBlockingPool();
}

base::FilePath IOSChromeTabRestoreServiceClient::GetPathToSaveTo() {
  // Note that this will return a different path in incognito from normal mode.
  // In this case, that shouldn't be an issue because the tab restore service
  // is not used in incognito mode.
  return browser_state_->GetStatePath();
}

GURL IOSChromeTabRestoreServiceClient::GetNewTabURL() {
  return GURL(kChromeUINewTabURL);
}

bool IOSChromeTabRestoreServiceClient::HasLastSession() {
  return false;
}

void IOSChromeTabRestoreServiceClient::GetLastSession(
    const sessions::GetLastSessionCallback& callback,
    base::CancelableTaskTracker* tracker) {
  NOTREACHED();
}
