// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

class MockStatusIcon : public StatusIcon {
  virtual void SetImage(const SkBitmap& image) {}
  virtual void SetToolTip(const string16& tool_tip) {}
  virtual void AddObserver(StatusIcon::StatusIconObserver* observer) {}
  virtual void RemoveObserver(StatusIcon::StatusIconObserver* observer) {}
};

class TestStatusIconFactory : public StatusIconFactory {
 public:
  MOCK_METHOD0(CreateIcon, StatusIcon*());
};

TEST(StatusTrayTest, Create) {
  // Check for creation and leaks.
  TestStatusIconFactory* factory = new TestStatusIconFactory();
  StatusTray tray(factory);
  EXPECT_CALL(*factory, CreateIcon()).WillOnce(Return(new MockStatusIcon()));
  tray.GetStatusIcon(ASCIIToUTF16("test"));
}

TEST(StatusTrayTest, GetIconTwice) {
  TestStatusIconFactory* factory = new TestStatusIconFactory();
  StatusTray tray(factory);
  string16 id = ASCIIToUTF16("test");
  // We should not try to create a new icon if we get the same ID twice.
  EXPECT_CALL(*factory, CreateIcon()).WillOnce(Return(new MockStatusIcon()));
  StatusIcon* icon =  tray.GetStatusIcon(id);
  EXPECT_EQ(icon, tray.GetStatusIcon(id));
}

TEST(StatusTrayTest, GetIconAfterRemove) {
  TestStatusIconFactory* factory = new TestStatusIconFactory();
  StatusTray tray(factory);
  string16 id = ASCIIToUTF16("test");
  EXPECT_CALL(*factory, CreateIcon()).Times(2)
      .WillOnce(Return(new MockStatusIcon()))
      .WillOnce(Return(new MockStatusIcon()));
  StatusIcon* icon =  tray.GetStatusIcon(id);
  EXPECT_EQ(icon, tray.GetStatusIcon(id));

  // If we remove the icon, then we should create a new one the next time in.
  tray.RemoveStatusIcon(id);
  tray.GetStatusIcon(id);
}
