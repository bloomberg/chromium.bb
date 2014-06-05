// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"

using content::RenderViewHost;
using content::WebContents;
using extensions::Extension;

class GtalkExtensionTest : public ExtensionBrowserTest {
 protected:
  extensions::ProcessManager* GetProcessManager() {
    return extensions::ExtensionSystem::Get(browser()->profile())->
        process_manager();
  }

  void InstallGtalkExtension(const std::string& version) {
    const Extension* extension = InstallExtensionWithUIAutoConfirm(
        test_data_dir_.AppendASCII("gtalk").AppendASCII(version + ".crx"),
        1, browser());
    installed_extension_id_ = extension->id();
  }

  const std::string& GetInstalledExtensionId() {
    return installed_extension_id_;
  }

  RenderViewHost* GetViewer() {
    std::vector<RenderViewHost*> views = GetMatchingViews(GetViewerUrl());
    EXPECT_EQ(1U, views.size());
    if (views.empty())
      return NULL;
    return views.front();
  }

  std::string GetViewerUrl() {
    return std::string(extensions::kExtensionScheme) +
        url::kStandardSchemeSeparator + GetInstalledExtensionId() +
        "/viewer.html";
  }

  std::vector<RenderViewHost*> GetMatchingViews(const std::string& url_query) {
    extensions::ProcessManager* manager = GetProcessManager();
    extensions::ProcessManager::ViewSet all_views = manager->GetAllViews();
    std::vector<RenderViewHost*> matching_views;
    for (extensions::ProcessManager::ViewSet::const_iterator iter =
         all_views.begin(); iter != all_views.end(); ++iter) {
      WebContents* web_contents = WebContents::FromRenderViewHost(*iter);
      std::string url = web_contents->GetURL().spec();
      if (url.find(url_query) != std::string::npos)
        matching_views.push_back(*iter);
    }
    return matching_views;
  }

  std::string ReadCurrentVersion() {
    std::string response;
    EXPECT_TRUE(base::ReadFileToString(
      test_data_dir_.AppendASCII("gtalk").AppendASCII("current_version"),
      &response));
    return response;
  }

 private:
  std::string installed_extension_id_;
};

IN_PROC_BROWSER_TEST_F(GtalkExtensionTest, InstallCurrent) {
  content::WindowedNotificationObserver panel_loaded(
    chrome::NOTIFICATION_EXTENSION_VIEW_REGISTERED,
    content::NotificationService::AllSources());
  InstallGtalkExtension(ReadCurrentVersion());
  panel_loaded.Wait();
  ASSERT_TRUE(GetViewer());
}
