// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/views/status_icons/status_icon_win.h"
#include "chrome/browser/ui/views/status_icons/status_tray_win.h"
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

TEST(StatusTrayWinTest, CreateTray) {
  // Just tests creation/destruction.
  StatusTrayWin tray;
}

TEST(StatusTrayWinTest, CreateIconAndMenu) {
  // Create an icon, set the images, tooltip, and context menu, then shut it
  // down.
  StatusTrayWin tray;
  StatusIcon* icon = tray.CreateStatusIcon();
  SkBitmap* bitmap = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_STATUS_TRAY_ICON);
  icon->SetImage(*bitmap);
  icon->SetPressedImage(*bitmap);
  icon->SetToolTip(ASCIIToUTF16("tool tip"));
  ui::SimpleMenuModel* menu = new ui::SimpleMenuModel(NULL);
  menu->AddItem(0, L"foo");
  icon->SetContextMenu(menu);
}

TEST(StatusTrayWinTest, ClickOnIcon) {
  // Create an icon, send a fake click event, make sure observer is called.
  StatusTrayWin tray;
  StatusIconWin* icon = static_cast<StatusIconWin*>(tray.CreateStatusIcon());
  MockStatusIconObserver observer;
  icon->AddObserver(&observer);
  EXPECT_CALL(observer, OnClicked());
  // Mimic a click.
  tray.WndProc(NULL, icon->message_id(), icon->icon_id(), WM_LBUTTONDOWN);
  // Mimic a right-click - observer should not be called.
  tray.WndProc(NULL, icon->message_id(), icon->icon_id(), WM_RBUTTONDOWN);
  icon->RemoveObserver(&observer);
}
