// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/tab_restore_service_delegate_provider_impl.h"

#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/ios/ios_live_tab.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sessions/tab_restore_service_delegate_impl_ios.h"
#include "ios/chrome/browser/sessions/tab_restore_service_delegate_impl_ios_factory.h"
#include "ios/chrome/browser/tabs/tab.h"
#include "ios/chrome/browser/tabs/tab_model.h"
#include "ios/chrome/browser/ui/browser_ios.h"
#include "ios/chrome/browser/ui/browser_list_ios.h"
#import "ios/web/public/web_state/web_state.h"

namespace ios_internal {

TabRestoreServiceDelegateProviderImpl::TabRestoreServiceDelegateProviderImpl() {
}

TabRestoreServiceDelegateProviderImpl::
    ~TabRestoreServiceDelegateProviderImpl() {}

sessions::LiveTabContext* TabRestoreServiceDelegateProviderImpl::Create(
    ios::ChromeBrowserState* browser_state) {
  return TabRestoreServiceDelegateImplIOSFactory::GetForBrowserState(
      browser_state);
}

sessions::LiveTabContext*
TabRestoreServiceDelegateProviderImpl::FindContextWithID(int32_t desired_id) {
  for (BrowserListIOS::const_iterator iter = BrowserListIOS::begin();
       iter != BrowserListIOS::end(); ++iter) {
    id<BrowserIOS> browser = *iter;
    if ([browser tabModel].sessionID.id() == desired_id) {
      return TabRestoreServiceDelegateImplIOSFactory::GetForBrowserState(
          [browser browserState]);
    }
  }
  return NULL;
}

sessions::LiveTabContext*
TabRestoreServiceDelegateProviderImpl::FindContextForTab(
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
  return NULL;
}

}  // namespace ios_internal
