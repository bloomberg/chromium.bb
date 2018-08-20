// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {

class PendingBookmarkAppManagerBrowserTest : public InProcessBrowserTest {};

// Basic integration test to make sure the whole flow works. Each step in the
// flow is unit tested separately.
IN_PROC_BROWSER_TEST_F(PendingBookmarkAppManagerBrowserTest, InstallSucceeds) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::RunLoop run_loop;
  std::string app_id;

  web_app::WebAppProvider::Get(browser()->profile())
      ->pending_app_manager()
      .Install(web_app::PendingAppManager::AppInfo(
                   embedded_test_server()->GetURL(
                       "/banners/manifest_test_page.html"),
                   web_app::PendingAppManager::LaunchContainer::kWindow),
               base::BindLambdaForTesting(
                   [&run_loop, &app_id](const GURL& provided_url,
                                        const std::string& id) {
                     app_id = id;
                     run_loop.QuitClosure().Run();
                   }));
  run_loop.Run();

  const Extension* app = ExtensionRegistry::Get(browser()->profile())
                             ->enabled_extensions()
                             .GetByID(app_id);
  ASSERT_TRUE(app);
  EXPECT_EQ("Manifest test app", app->name());
}

}  // namespace extensions
