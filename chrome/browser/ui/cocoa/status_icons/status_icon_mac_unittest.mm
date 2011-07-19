// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/status_icons/status_icon_mac.h"
#include "grit/theme_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

class SkBitmap;

class StatusIconMacTest : public CocoaTest {
};

TEST_F(StatusIconMacTest, Create) {
  // Create an icon, set the tool tip, then shut it down (checks for leaks).
  scoped_ptr<StatusIcon> icon(new StatusIconMac());
  SkBitmap* bitmap = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_STATUS_TRAY_ICON);
  icon->SetImage(*bitmap);
  SkBitmap* pressed = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_STATUS_TRAY_ICON_PRESSED);
  icon->SetPressedImage(*pressed);
  icon->SetToolTip(ASCIIToUTF16("tool tip"));
}
