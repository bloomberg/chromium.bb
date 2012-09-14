// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/tabs/throbber_view.h"
#include "grit/ui_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {

class ThrobberViewTest : public CocoaTest {
 public:
  ThrobberViewTest() {
    NSRect frame = NSMakeRect(10, 10, 16, 16);
    NSImage* image = ResourceBundle::GetSharedInstance().GetNativeImageNamed(
        IDR_THROBBER).ToNSImage();
    view_ = [ThrobberView filmstripThrobberViewWithFrame:frame image:image];
    [[test_window() contentView] addSubview:view_];
  }

  ThrobberView* view_;
};

TEST_VIEW(ThrobberViewTest, view_)

}  // namespace
