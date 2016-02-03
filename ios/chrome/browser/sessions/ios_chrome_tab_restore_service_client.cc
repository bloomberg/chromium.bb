// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_client.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/sessions/live_tab_context_provider.h"
#include "ios/web/public/web_thread.h"
#include "url/gurl.h"

IOSChromeTabRestoreServiceClient::IOSChromeTabRestoreServiceClient(
    ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {}

IOSChromeTabRestoreServiceClient::~IOSChromeTabRestoreServiceClient() {}

sessions::LiveTabContext*
IOSChromeTabRestoreServiceClient::CreateLiveTabContext(
    int host_desktop_type,
    const std::string& app_name) {
  return ios::GetChromeBrowserProvider()->GetLiveTabContextProvider()->Create(
      browser_state_);
}

sessions::LiveTabContext*
IOSChromeTabRestoreServiceClient::FindLiveTabContextForTab(
    const sessions::LiveTab* tab) {
  return ios::GetChromeBrowserProvider()
      ->GetLiveTabContextProvider()
      ->FindContextForTab(tab);
}

sessions::LiveTabContext*
IOSChromeTabRestoreServiceClient::FindLiveTabContextWithID(
    SessionID::id_type desired_id,
    int host_desktop_type) {
  return ios::GetChromeBrowserProvider()
      ->GetLiveTabContextProvider()
      ->FindContextWithID(desired_id);
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
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
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
