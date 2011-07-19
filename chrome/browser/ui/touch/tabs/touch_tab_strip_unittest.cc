// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/tabs/touch_tab_strip.h"

#include "chrome/browser/ui/views/tabs/base_tab_strip_test_fixture.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/side_tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

// BaseTabStrip unit tests using TouchTabStrip.
INSTANTIATE_TYPED_TEST_CASE_P(TouchTabStrip,
                              BaseTabStripTestFixture,
                              TouchTabStrip);

class TouchTabStripTestFixture : public testing::Test {
 public:
  TouchTabStripTestFixture()
      : tab_strip_(new FakeBaseTabStripController()) {
  }

 protected:
  TouchTabStrip* tab_strip() { return &tab_strip_; }

 private:
  TouchTabStrip tab_strip_;
};

TEST_F(TouchTabStripTestFixture, TouchTabStripIsHorizontal) {
  EXPECT_EQ(BaseTabStrip::HORIZONTAL_TAB_STRIP, tab_strip()->type());
}

