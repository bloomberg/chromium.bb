// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_BASE_TAB_STRIP_TEST_FIXTURE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_BASE_TAB_STRIP_TEST_FIXTURE_H_
#pragma once

#include "chrome/browser/ui/views/tabs/base_tab_strip.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

template <class TAB_STRIP_TYPE>
class BaseTabStripTestFixture : public testing::Test {
 public:
  BaseTabStripTestFixture()
      : tab_strip_(new FakeBaseTabStripController()) {
  }

 protected:
  BaseTabStrip* tab_strip() { return &tab_strip_; }

 private:
  TAB_STRIP_TYPE tab_strip_;
};

TYPED_TEST_CASE_P(BaseTabStripTestFixture);

// NOTE: The macros hide the fact that this is in fact a template class
// definition, and it inherits from another template base class, therefore
// unqualified names will not resolve resolve without the `this->` specifier.
// `foo()` doesn't resolve, but `this->foo()` does.
TYPED_TEST_P(BaseTabStripTestFixture, GetModelCount) {
  EXPECT_EQ(0, this->tab_strip()->GetModelCount());
}

TYPED_TEST_P(BaseTabStripTestFixture, IsValidModelIndex) {
  EXPECT_FALSE(this->tab_strip()->IsValidModelIndex(0));
}

TYPED_TEST_P(BaseTabStripTestFixture, tab_count) {
  EXPECT_EQ(0, this->tab_strip()->tab_count());
}

TYPED_TEST_P(BaseTabStripTestFixture, CreateTabForDragging) {
  // Any result is good, as long as it doesn't crash.
  this->tab_strip()->CreateTabForDragging();
}


TYPED_TEST_P(BaseTabStripTestFixture, AddTabAt) {
  this->tab_strip()->AddTabAt(0, TabRendererData());
  EXPECT_EQ(1, this->tab_strip()->tab_count());
  BaseTab* tab = this->tab_strip()->base_tab_at_tab_index(0);
  EXPECT_FALSE(tab == NULL);
}

REGISTER_TYPED_TEST_CASE_P(BaseTabStripTestFixture,
                           GetModelCount,
                           IsValidModelIndex,
                           tab_count,
                           CreateTabForDragging,
                           AddTabAt);

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_BASE_TAB_STRIP_TEST_FIXTURE_H_

