// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_tooltip_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

namespace {

class AutofillTooltipControllerTest: public ui::CocoaTest {
 public:
  virtual void SetUp() OVERRIDE {
    CocoaTest::SetUp();
    controller_.reset([[AutofillTooltipController alloc]
                           initWithArrowLocation:info_bubble::kTopCenter]);
    [[test_window() contentView] addSubview:[controller_ view]];
  }

 protected:
  base::scoped_nsobject<AutofillTooltipController> controller_;
};

}  // namespace

TEST_VIEW(AutofillTooltipControllerTest, [controller_ view])
