// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

class ContentSettingBubbleModelMixedScriptTest : public InProcessBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
    ASSERT_TRUE(https_server_->Start());
  }

  TabSpecificContentSettings* GetActiveTabSpecificContentSettings() {
    return TabSpecificContentSettings::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  scoped_ptr<net::EmbeddedTestServer> https_server_;
};

// Tests that a MIXEDSCRIPT type ContentSettingBubbleModel sends appropriate
// IPCs to allow the renderer to load unsafe scripts and refresh the page
// automatically.
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleModelMixedScriptTest, MainFrame) {
  GURL url(https_server_->GetURL("/content_setting_bubble/mixed_script.html"));

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

// Tests that a MIXEDSCRIPT type ContentSettingBubbleModel does not work
// for an iframe (mixed script in iframes is never allowed and the mixed
// content shield isn't shown for it).
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleModelMixedScriptTest, Iframe) {
  GURL url(https_server_->GetURL(
      "/content_setting_bubble/mixed_script_in_iframe.html"));

  ui_test_utils::NavigateToURL(browser(), url);

  // Blink does not ask the browser to handle mixed content in the case
  // of active subresources in an iframe, so the content type should not
  // be marked as blocked.
  EXPECT_FALSE(GetActiveTabSpecificContentSettings()->IsContentBlocked(
      CONTENT_SETTINGS_TYPE_MIXEDSCRIPT));
}

class ContentSettingBubbleModelMediaStreamTest : public InProcessBrowserTest {
 public:
  void ManageMediaStreamSettings(
      TabSpecificContentSettings::MicrophoneCameraState state) {
    // Open a tab for which we will invoke the media bubble.
    GURL url(ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("content_setting_bubble"),
        base::FilePath().AppendASCII("mixed_script.html")));
    ui_test_utils::NavigateToURL(browser(), url);
    content::WebContents* original_tab = GetActiveTab();

    // Create a bubble with the given camera and microphone access state.
    TabSpecificContentSettings::FromWebContents(original_tab)->
        OnMediaStreamPermissionSet(
            original_tab->GetLastCommittedURL(),
            state, std::string(), std::string(), std::string(), std::string());
    scoped_ptr<ContentSettingBubbleModel> bubble(
        new ContentSettingMediaStreamBubbleModel(
            browser()->content_setting_bubble_model_delegate(),
            original_tab,
            browser()->profile()));

    // Click the management link, which opens in a new tab or window.
    // Wait until it loads.
    bubble->OnManageLinkClicked();
    ASSERT_NE(GetActiveTab(), original_tab);
    content::TestNavigationObserver observer(GetActiveTab());
    observer.Wait();
  }

  content::WebContents* GetActiveTab() {
    // First, we need to find the active browser window. It should be at
    // the same desktop as the browser in which we invoked the bubble.
    Browser* active_browser = chrome::FindLastActive();
    return active_browser->tab_strip_model()->GetActiveWebContents();
  }
};

// Tests that clicking on the management link in the media bubble opens
// the correct section of the settings UI.
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleModelMediaStreamTest, ManageLink) {
  // For each of the three options, we click the management link and check if
  // the active tab loads the correct internal url.

  // The microphone bubble links to microphone exceptions.
  ManageMediaStreamSettings(TabSpecificContentSettings::MICROPHONE_ACCESSED);
  EXPECT_EQ(GURL("chrome://settings/contentExceptions#media-stream-mic"),
            GetActiveTab()->GetLastCommittedURL());

  // The camera bubble links to camera exceptions.
  ManageMediaStreamSettings(TabSpecificContentSettings::CAMERA_ACCESSED);
  EXPECT_EQ(GURL("chrome://settings/contentExceptions#media-stream-camera"),
            GetActiveTab()->GetLastCommittedURL());

  // The bubble for both media devices links to the the first section of the
  // default media content settings, which is the microphone section.
  ManageMediaStreamSettings(TabSpecificContentSettings::MICROPHONE_ACCESSED |
                            TabSpecificContentSettings::CAMERA_ACCESSED);
  EXPECT_EQ(GURL("chrome://settings/content#media-stream-mic"),
            GetActiveTab()->GetLastCommittedURL());
}
