// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/one_click_signin_bubble_gtk.h"

#include <gtk/gtk.h>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class OneClickSigninBubbleGtkTest : public InProcessBrowserTest {
 public:
  OneClickSigninBubbleGtkTest()
      : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        learn_more_callback_(
            base::Bind(&OneClickSigninBubbleGtkTest::OnLearnMore,
                       weak_ptr_factory_.GetWeakPtr())),
        advanced_callback_(
            base::Bind(&OneClickSigninBubbleGtkTest::OnAdvanced,
                       weak_ptr_factory_.GetWeakPtr())),
        bubble_(NULL) {}

  virtual OneClickSigninBubbleGtk* MakeBubble() {
    return new OneClickSigninBubbleGtk(
        static_cast<BrowserWindowGtk*>(browser()->window()),
        learn_more_callback_,
        advanced_callback_);
  }

  MOCK_METHOD0(OnLearnMore, void());
  MOCK_METHOD0(OnAdvanced, void());

 protected:
  base::WeakPtrFactory<OneClickSigninBubbleGtkTest> weak_ptr_factory_;
  base::Closure learn_more_callback_;
  base::Closure advanced_callback_;

  // Owns itself.
  OneClickSigninBubbleGtk* bubble_;
};

// Test that the dialog doesn't call any callback if the OK button is
// clicked.
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleGtkTest, ShowAndOK) {
  EXPECT_CALL(*this, OnLearnMore()).Times(0);
  EXPECT_CALL(*this, OnAdvanced()).Times(0);

  MakeBubble()->ClickOKForTest();
}

// Test that the learn more callback is run if its corresponding
// button is clicked.
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleGtkTest, ShowAndClickLearnMore) {
  EXPECT_CALL(*this, OnLearnMore()).Times(1);
  EXPECT_CALL(*this, OnAdvanced()).Times(0);

  MakeBubble()->ClickLearnMoreForTest();
}

// Test that the advanced callback is run if its corresponding button
// is clicked.
IN_PROC_BROWSER_TEST_F(OneClickSigninBubbleGtkTest, ShowAndClickAdvanced) {
  EXPECT_CALL(*this, OnLearnMore()).Times(0);
  EXPECT_CALL(*this, OnAdvanced()).Times(1);

  MakeBubble()->ClickAdvancedForTest();
}

}  // namespace
