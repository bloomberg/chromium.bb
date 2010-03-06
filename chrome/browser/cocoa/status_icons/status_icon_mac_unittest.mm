// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/resource_bundle.h"
#include "base/string_util.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/cocoa/status_icons/status_icon_mac.h"
#include "grit/browser_resources.h"
#include "grit/theme_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class SkBitmap;

class MockStatusIconObserver : public StatusIcon::StatusIconObserver {
 public:
  MOCK_METHOD0(OnClicked, void());
};

class StatusIconMacTest : public CocoaTest {
};

TEST_F(StatusIconMacTest, Create) {
  // Create an icon, set the tool tip, then shut it down (checks for leaks).
  scoped_ptr<StatusIcon> icon(new StatusIconMac());
  SkBitmap* bitmap = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_STATUS_TRAY_ICON);
  icon->SetImage(*bitmap);
  icon->SetToolTip(ASCIIToUTF16("tool tip"));
}

TEST_F(StatusIconMacTest, ObserverAdd) {
  // Make sure that observers are invoked when we click items.
  StatusIconMac icon;
  MockStatusIconObserver observer, observer2;
  EXPECT_CALL(observer, OnClicked()).Times(2);
  EXPECT_CALL(observer2, OnClicked());
  icon.AddObserver(&observer);
  icon.HandleClick();
  icon.AddObserver(&observer2);
  icon.HandleClick();
  icon.RemoveObserver(&observer);
  icon.RemoveObserver(&observer2);
}

TEST_F(StatusIconMacTest, ObserverRemove) {
  // Make sure that observers are no longer invoked after they are removed.
  StatusIconMac icon;
  MockStatusIconObserver observer;
  EXPECT_CALL(observer, OnClicked());
  icon.AddObserver(&observer);
  icon.HandleClick();
  icon.RemoveObserver(&observer);
  icon.HandleClick();
}

