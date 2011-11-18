// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/status_icons/status_icon_chromeos.h"
#include "chrome/browser/ui/views/status_icons/status_tray_chromeos.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "grit/theme_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"

class SkBitmap;

class MockStatusIconObserver : public StatusIcon::Observer {
 public:
  MOCK_METHOD0(OnClicked, void());
};

// Using a browser test since in ChromeOS the status icons are added
// to the browser windows. If there is no browser nothing is tested.
class StatusTrayChromeOSBrowserTest : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(StatusTrayChromeOSBrowserTest, CreateTray) {
  // Just tests creation/destruction.
  StatusTrayChromeOS tray;
}

IN_PROC_BROWSER_TEST_F(StatusTrayChromeOSBrowserTest, CreateIconAndMenu) {
  // Create an icon, set the images, tooltip, and context menu, then shut it
  // down.
  StatusTrayChromeOS tray;
  StatusIcon* icon = tray.CreateStatusIcon();
  SkBitmap* bitmap = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_STATUS_TRAY_ICON);
  icon->SetImage(*bitmap);
  icon->SetPressedImage(*bitmap);
  icon->SetToolTip(ASCIIToUTF16("tool tip"));
  ui::SimpleMenuModel* menu = new ui::SimpleMenuModel(NULL);
  menu->AddItem(0, ASCIIToUTF16("foo"));
  icon->SetContextMenu(menu);

  Browser* browser = BrowserList::GetLastActive();
  ASSERT_TRUE(browser != NULL);
  chromeos::BrowserView* browser_view =
      chromeos::BrowserView::GetBrowserViewForBrowser(browser);
  ASSERT_TRUE(browser_view != NULL);
  StatusAreaButton* button =
      static_cast<StatusIconChromeOS*>(icon)->GetButtonForBrowser(browser);
  ASSERT_TRUE(button != NULL);
  ASSERT_TRUE(browser_view->ContainsButton(button));
}

IN_PROC_BROWSER_TEST_F(StatusTrayChromeOSBrowserTest, ClickOnIcon) {
  // Create an icon, send a fake click event, make sure observer is called.
  StatusTrayChromeOS tray;
  StatusIconChromeOS* icon =
      static_cast<StatusIconChromeOS*>(tray.CreateStatusIcon());
  MockStatusIconObserver observer;
  icon->AddObserver(&observer);
  EXPECT_CALL(observer, OnClicked());

  // Send a click.
  Browser* browser = BrowserList::GetLastActive();
  ASSERT_TRUE(browser != NULL);
  StatusAreaButton* button =
      static_cast<StatusIconChromeOS*>(icon)->GetButtonForBrowser(browser);
  ASSERT_TRUE(button != NULL);
  button->Activate();
  icon->RemoveObserver(&observer);
}

