// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_ui_delegate_impl.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace web_app {

using WebAppUiDelegateImplBrowsertest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(WebAppUiDelegateImplBrowsertest, NoAppWindows) {
  // Should not crash.
  auto& delegate = WebAppProvider::Get(browser()->profile())->ui_delegate();
  auto* delegate_impl = WebAppUiDelegateImpl::Get(browser()->profile());
  EXPECT_EQ(&delegate, delegate_impl);

  // Zero apps on start:
  EXPECT_EQ(0u, delegate.GetNumWindowsForApp(AppId()));
}

}  // namespace web_app
