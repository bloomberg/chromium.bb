// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class ToolbarViewTest : public CocoaTest {
};

// This class only needs to do one thing: prevent mouse down events from moving
// the parent window around.
TEST_F(ToolbarViewTest, CanDragWindow) {
  scoped_nsobject<ToolbarView> view([[ToolbarView alloc] init]);
  EXPECT_FALSE([view mouseDownCanMoveWindow]);
}

}  // namespace
