// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/status_icons/status_tray_gtk.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_icon_observer.h"
#include "chrome/browser/ui/gtk/status_icons/status_icon_gtk.h"
#include "grit/chrome_unscaled_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace {

class MockStatusIconObserver : public StatusIconObserver {
 public:
  MOCK_METHOD0(OnStatusIconClicked, void());
};

TEST(StatusTrayGtkTest, CreateTray) {
  // Just tests creation/destruction.
  StatusTrayGtk tray;
}

TEST(StatusTrayGtkTest, CreateIcon) {
  // Create an icon, set the images and tooltip, then shut it down.
  StatusTrayGtk tray;
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* image = rb.GetImageSkiaNamed(IDR_STATUS_TRAY_ICON);
  StatusIcon* icon = tray.CreateStatusIcon(
      StatusTray::OTHER_ICON, *image, ASCIIToUTF16("tool tip"));
  icon->SetPressedImage(*image);
  scoped_ptr<StatusIconMenuModel> menu(new StatusIconMenuModel(NULL));
  menu->AddItem(0, ASCIIToUTF16("foo"));
  icon->SetContextMenu(menu.Pass());
}

TEST(StatusTrayGtkTest, ClickOnIcon) {
  // Create an icon, send a fake click event, make sure observer is called.
  StatusTrayGtk tray;
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* image = rb.GetImageSkiaNamed(IDR_STATUS_TRAY_ICON);
  StatusIconGtk* icon = static_cast<StatusIconGtk*>(tray.CreateStatusIcon(
      StatusTray::OTHER_ICON, *image, ASCIIToUTF16("tool tip")));
  MockStatusIconObserver observer;
  icon->AddObserver(&observer);
  EXPECT_CALL(observer, OnStatusIconClicked());
  // Mimic a click.
  icon->OnClick(NULL);
  icon->RemoveObserver(&observer);
}

}  // namespace
