// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/status_icon.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockStatusIconObserver : public StatusIcon::Observer {
 public:
  MOCK_METHOD0(OnClicked, void());
};

// Define pure virtual functions so we can test base class functionality.
class TestStatusIcon : public StatusIcon {
 public:
  TestStatusIcon() {}
  virtual void SetImage(const SkBitmap& image) {}
  virtual void SetPressedImage(const SkBitmap& image) {}
  virtual void SetToolTip(const string16& tool_tip) {}
  virtual void UpdatePlatformContextMenu(ui::MenuModel* menu) {}
  virtual void DisplayBalloon(const string16& title,
                              const string16& contents) {}
};

TEST(StatusIconTest, ObserverAdd) {
  // Make sure that observers are invoked when we click items.
  TestStatusIcon icon;
  MockStatusIconObserver observer, observer2;
  EXPECT_CALL(observer, OnClicked()).Times(2);
  EXPECT_CALL(observer2, OnClicked());
  icon.AddObserver(&observer);
  icon.DispatchClickEvent();
  icon.AddObserver(&observer2);
  icon.DispatchClickEvent();
  icon.RemoveObserver(&observer);
  icon.RemoveObserver(&observer2);
}

TEST(StatusIconTest, ObserverRemove) {
  // Make sure that observers are no longer invoked after they are removed.
  TestStatusIcon icon;
  MockStatusIconObserver observer;
  EXPECT_CALL(observer, OnClicked());
  icon.AddObserver(&observer);
  icon.DispatchClickEvent();
  icon.RemoveObserver(&observer);
  icon.DispatchClickEvent();
}
