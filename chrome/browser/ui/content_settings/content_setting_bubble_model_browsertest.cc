// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

class ContentSettingBubbleModelMixedScriptTest : public InProcessBrowserTest {
 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    https_server_.reset(
        new net::SpawnedTestServer(
            net::SpawnedTestServer::TYPE_HTTPS,
            net::SpawnedTestServer::SSLOptions(
                net::SpawnedTestServer::SSLOptions::CERT_OK),
            base::FilePath(kDocRoot)));
    ASSERT_TRUE(https_server_->Start());
  }

  TabSpecificContentSettings* GetActiveTabSpecificContentSettings() {
    return TabSpecificContentSettings::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  scoped_ptr<net::SpawnedTestServer> https_server_;
};

// Tests that a MIXEDSCRIPT type ContentSettingBubbleModel sends appropriate
// IPCs to allow the renderer to load unsafe scripts and refresh the page
// automatically.
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleModelMixedScriptTest, MainFrame) {
  GURL url(https_server_->GetURL(
      "files/content_setting_bubble/mixed_script.html"));

  // Load a page with mixed content and do quick verification by looking at
  // the title string.
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_TRUE(GetActiveTabSpecificContentSettings()->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MIXEDSCRIPT));

  // Emulate link clicking on the mixed script bubble.
  scoped_ptr<ContentSettingBubbleModel> model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          browser()->content_setting_bubble_model_delegate(),
          browser()->tab_strip_model()->GetActiveWebContents(),
          browser()->profile(),
          CONTENT_SETTINGS_TYPE_MIXEDSCRIPT));
  model->OnCustomLinkClicked();

  // Wait for reload
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  observer.Wait();

  EXPECT_FALSE(GetActiveTabSpecificContentSettings()->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MIXEDSCRIPT));
}

// Tests that a MIXEDSCRIPT type ContentSettingBubbleModel works for an iframe.
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleModelMixedScriptTest, Iframe) {
  GURL url(https_server_->GetURL(
      "files/content_setting_bubble/mixed_script_in_iframe.html"));

  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_TRUE(GetActiveTabSpecificContentSettings()->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MIXEDSCRIPT));

  scoped_ptr<ContentSettingBubbleModel> model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          browser()->content_setting_bubble_model_delegate(),
          browser()->tab_strip_model()->GetActiveWebContents(),
          browser()->profile(),
          CONTENT_SETTINGS_TYPE_MIXEDSCRIPT));
  model->OnCustomLinkClicked();

  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  observer.Wait();

  EXPECT_FALSE(GetActiveTabSpecificContentSettings()->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MIXEDSCRIPT));
}
