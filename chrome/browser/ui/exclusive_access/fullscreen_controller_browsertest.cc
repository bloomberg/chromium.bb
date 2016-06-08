// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::WebContents;
using ui::PAGE_TRANSITION_TYPED;

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest, MouseLockOnFileURL) {
  static const base::FilePath::CharType* kEmptyFile =
      FILE_PATH_LITERAL("empty.html");
  GURL file_url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kEmptyFile)));
  AddTabAtIndex(0, file_url, PAGE_TRANSITION_TYPED);
  RequestToLockMouse(true, false);
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest, FullscreenOnFileURL) {
  static const base::FilePath::CharType* kEmptyFile =
      FILE_PATH_LITERAL("empty.html");
  GURL file_url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kEmptyFile)));
  AddTabAtIndex(0, file_url, PAGE_TRANSITION_TYPED);
  GetFullscreenController()->EnterFullscreenModeForTab(
      browser()->tab_strip_model()->GetActiveWebContents(),
      file_url.GetOrigin());
  ASSERT_TRUE(IsFullscreenBubbleDisplayed());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest, PermissionContentSettings) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(kFullscreenMouseLockHTML);
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_FALSE(browser()->window()->IsFullscreen());

  // The content's origin is not allowed to go fullscreen.
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      HostContentSettingsMapFactory::GetForProfile(browser()->profile())
          ->GetContentSetting(url.GetOrigin(),
                              url.GetOrigin(),
                              CONTENT_SETTINGS_TYPE_FULLSCREEN,
                              std::string()));

  GetFullscreenController()->EnterFullscreenModeForTab(
      browser()->tab_strip_model()->GetActiveWebContents(), url.GetOrigin());
  EXPECT_TRUE(IsFullscreenBubbleDisplayed());

  // The content's origin is still not allowed to go fullscreen.
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      HostContentSettingsMapFactory::GetForProfile(browser()->profile())
          ->GetContentSetting(url.GetOrigin(),
                              url.GetOrigin(),
                              CONTENT_SETTINGS_TYPE_FULLSCREEN,
                              std::string()));

  // The primary and secondary patterns have been set when setting the
  // permission, thus setting another secondary pattern shouldn't work.
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      HostContentSettingsMapFactory::GetForProfile(browser()->profile())
          ->GetContentSetting(url.GetOrigin(),
                              GURL("https://test.com"),
                              CONTENT_SETTINGS_TYPE_FULLSCREEN,
                              std::string()));

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(CONTENT_SETTINGS_TYPE_FULLSCREEN);
}
