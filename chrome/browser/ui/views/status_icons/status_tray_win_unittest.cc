// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_tray_win.h"

#include <commctrl.h>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_icon_observer.h"
#include "chrome/browser/ui/views/status_icons/status_icon_win.h"
#include "grit/chrome_unscaled_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

class FakeStatusIconObserver : public StatusIconObserver {
 public:
  FakeStatusIconObserver()
      : balloon_clicked_(false),
        status_icon_click_count_(0) {}
  virtual void OnStatusIconClicked() {
    ++status_icon_click_count_;
  }
  virtual void OnBalloonClicked() { balloon_clicked_ = true; }
  bool balloon_clicked() const { return balloon_clicked_; }
  size_t status_icon_click_count() const {
    return status_icon_click_count_;
  }

 private:
  size_t status_icon_click_count_;
  bool balloon_clicked_;
};

TEST(StatusTrayWinTest, CreateTray) {
  // Just tests creation/destruction.
  StatusTrayWin tray;
}

TEST(StatusTrayWinTest, CreateIconAndMenu) {
  // Create an icon, set the images, tooltip, and context menu, then shut it
  // down.
  StatusTrayWin tray;
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* image = rb.GetImageSkiaNamed(IDR_STATUS_TRAY_ICON);
  StatusIcon* icon = tray.CreateStatusIcon(
      StatusTray::OTHER_ICON, *image, ASCIIToUTF16("tool tip"));
  icon->SetPressedImage(*image);
  scoped_ptr<StatusIconMenuModel> menu(new StatusIconMenuModel(NULL));
  menu->AddItem(0, L"foo");
  icon->SetContextMenu(menu.Pass());
}

#if !defined(USE_AURA)  // http://crbug.com/156370
TEST(StatusTrayWinTest, ClickOnIcon) {
  // Create an icon, send a fake click event, make sure observer is called.
  StatusTrayWin tray;
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* image = rb.GetImageSkiaNamed(IDR_STATUS_TRAY_ICON);

  StatusIconWin* icon = static_cast<StatusIconWin*>(tray.CreateStatusIcon(
      StatusTray::OTHER_ICON, *image, ASCIIToUTF16("tool tip")));
  FakeStatusIconObserver observer;
  icon->AddObserver(&observer);
  // Mimic a click.
  tray.WndProc(NULL, icon->message_id(), icon->icon_id(), WM_LBUTTONDOWN);
  // Mimic a right-click - observer should not be called.
  tray.WndProc(NULL, icon->message_id(), icon->icon_id(), WM_RBUTTONDOWN);
  EXPECT_EQ(1, observer.status_icon_click_count());
  icon->RemoveObserver(&observer);
}

TEST(StatusTrayWinTest, ClickOnBalloon) {
  // Create an icon, send a fake click event, make sure observer is called.
  StatusTrayWin tray;
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* image = rb.GetImageSkiaNamed(IDR_STATUS_TRAY_ICON);

  StatusIconWin* icon = static_cast<StatusIconWin*>(tray.CreateStatusIcon(
      StatusTray::OTHER_ICON, *image, ASCIIToUTF16("tool tip")));
  FakeStatusIconObserver observer;
  icon->AddObserver(&observer);
  // Mimic a click.
  tray.WndProc(
      NULL, icon->message_id(), icon->icon_id(), TB_INDETERMINATE);
  EXPECT_TRUE(observer.balloon_clicked());
  icon->RemoveObserver(&observer);
}

TEST(StatusTrayWinTest, HandleOldIconId) {
  StatusTrayWin tray;
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* image = rb.GetImageSkiaNamed(IDR_STATUS_TRAY_ICON);

  StatusIconWin* icon = static_cast<StatusIconWin*>(tray.CreateStatusIcon(
      StatusTray::OTHER_ICON, *image, ASCIIToUTF16("tool tip")));
  UINT message_id = icon->message_id();
  UINT icon_id = icon->icon_id();

  tray.RemoveStatusIcon(icon);
  tray.WndProc(NULL, message_id, icon_id, WM_LBUTTONDOWN);
}
#endif  // !defined(USE_AURA)
