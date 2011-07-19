// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

class MockStatusIcon : public StatusIcon {
  virtual void SetImage(const SkBitmap& image) {}
  virtual void SetPressedImage(const SkBitmap& image) {}
  virtual void SetToolTip(const string16& tool_tip) {}
  virtual void DisplayBalloon(const string16& title,
                              const string16& contents) {}
  virtual void UpdatePlatformContextMenu(ui::MenuModel* menu) {}
  virtual void AddObserver(StatusIcon::Observer* observer) {}
  virtual void RemoveObserver(StatusIcon::Observer* observer) {}
};

class TestStatusTray : public StatusTray {
 public:
  MOCK_METHOD0(CreatePlatformStatusIcon, StatusIcon*());
  MOCK_METHOD1(UpdatePlatformContextMenu, void(ui::MenuModel*));
};

TEST(StatusTrayTest, Create) {
  // Check for creation and leaks.
  TestStatusTray tray;
  EXPECT_CALL(tray,
      CreatePlatformStatusIcon()).WillOnce(Return(new MockStatusIcon()));
  tray.CreateStatusIcon();
}

// Make sure that removing an icon removes it from the list.
TEST(StatusTrayTest, CreateRemove) {
  TestStatusTray tray;
  EXPECT_CALL(tray,
      CreatePlatformStatusIcon()).WillOnce(Return(new MockStatusIcon()));
  StatusIcon* icon = tray.CreateStatusIcon();
  EXPECT_EQ(1U, tray.status_icons_.size());
  tray.RemoveStatusIcon(icon);
  EXPECT_EQ(0U, tray.status_icons_.size());
  // Calling again should do nothing.
  tray.RemoveStatusIcon(icon);
}
