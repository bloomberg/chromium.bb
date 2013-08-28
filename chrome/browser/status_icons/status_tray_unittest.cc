// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "grit/chrome_unscaled_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

class MockStatusIcon : public StatusIcon {
  virtual void SetImage(const gfx::ImageSkia& image) OVERRIDE {}
  virtual void SetPressedImage(const gfx::ImageSkia& image) OVERRIDE {}
  virtual void SetToolTip(const string16& tool_tip) OVERRIDE {}
  virtual void DisplayBalloon(const gfx::ImageSkia& icon,
                              const string16& title,
                              const string16& contents) OVERRIDE {}
  virtual void UpdatePlatformContextMenu(
      StatusIconMenuModel* menu) OVERRIDE {}
};

class TestStatusTray : public StatusTray {
 public:
  virtual StatusIcon* CreatePlatformStatusIcon(
      StatusIconType type,
      const gfx::ImageSkia& image,
      const string16& tool_tip) OVERRIDE {
    return new MockStatusIcon();
  }

  const StatusIcons& GetStatusIconsForTest() const { return status_icons(); }
};

TEST(StatusTrayTest, Create) {
  // Check for creation and leaks.
  TestStatusTray tray;
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* image = rb.GetImageSkiaNamed(IDR_STATUS_TRAY_ICON);
  tray.CreateStatusIcon(
      StatusTray::OTHER_ICON, *image, ASCIIToUTF16("tool tip"));
  EXPECT_EQ(1U, tray.GetStatusIconsForTest().size());
}

// Make sure that removing an icon removes it from the list.
TEST(StatusTrayTest, CreateRemove) {
  TestStatusTray tray;
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* image = rb.GetImageSkiaNamed(IDR_STATUS_TRAY_ICON);
  StatusIcon* icon = tray.CreateStatusIcon(
      StatusTray::OTHER_ICON, *image, ASCIIToUTF16("tool tip"));
  EXPECT_EQ(1U, tray.GetStatusIconsForTest().size());
  tray.RemoveStatusIcon(icon);
  EXPECT_EQ(0U, tray.GetStatusIconsForTest().size());
}
