// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_DELEGATE_PROVIDER_IMPL_H_
#define IOS_CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_DELEGATE_PROVIDER_IMPL_H_

#include "ios/public/provider/chrome/browser/sessions/live_tab_context_provider.h"

namespace ios_internal {

class TabRestoreServiceDelegateProviderImpl
    : public ios::LiveTabContextProvider {
 public:
  TabRestoreServiceDelegateProviderImpl();
  ~TabRestoreServiceDelegateProviderImpl() override;
  sessions::LiveTabContext* Create(
      ios::ChromeBrowserState* browser_state) override;
  sessions::LiveTabContext* FindContextWithID(int32_t desired_id) override;
  sessions::LiveTabContext* FindContextForTab(
      const sessions::LiveTab* tab) override;
};

}  // namespace ios_internal

#endif  // IOS_CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_DELEGATE_PROVIDER_IMPL_H_
