// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_delegate_impl.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/web_app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_ui_delegate_impl_factory.h"
#include "chrome/browser/web_applications/web_app_provider.h"

namespace web_app {

// static
WebAppUiDelegateImpl* WebAppUiDelegateImpl::Get(Profile* profile) {
  return WebAppUiDelegateImplFactory::GetForProfile(profile);
}

WebAppUiDelegateImpl::WebAppUiDelegateImpl(Profile* profile)
    : profile_(profile) {
  WebAppProvider::Get(profile_)->set_ui_delegate(this);
}

WebAppUiDelegateImpl::~WebAppUiDelegateImpl() {
  WebAppProvider::Get(profile_)->set_ui_delegate(nullptr);
}

size_t WebAppUiDelegateImpl::GetNumWindowsForApp(const AppId& app_id) {
  size_t num_windows_for_app = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile_)
      continue;

    if (browser->web_app_controller()->GetAppId() == app_id)
      ++num_windows_for_app;
  }

  return num_windows_for_app;
}

}  // namespace web_app
