// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "chrome/browser/speech/speech_input_bubble.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"

class SpeechInputBubbleTest : public SpeechInputBubbleDelegate,
                              public InProcessBrowserTest {
 public:
  // SpeechInputBubble::Delegate methods.
  virtual void InfoBubbleButtonClicked(SpeechInputBubble::Button button) {}
  virtual void InfoBubbleFocusChanged() {}

 protected:
};

IN_PROC_BROWSER_TEST_F(SpeechInputBubbleTest, CreateAndDestroy) {
  gfx::Rect element_rect(100, 100, 100, 100);
  scoped_ptr<SpeechInputBubble> bubble(SpeechInputBubble::Create(
      browser()->GetSelectedTabContents(), this, element_rect));
  EXPECT_TRUE(bubble.get());
}

IN_PROC_BROWSER_TEST_F(SpeechInputBubbleTest, ShowAndDestroy) {
  gfx::Rect element_rect(100, 100, 100, 100);
  scoped_ptr<SpeechInputBubble> bubble(SpeechInputBubble::Create(
      browser()->GetSelectedTabContents(), this, element_rect));
  EXPECT_TRUE(bubble.get());
  bubble->Show();
}

IN_PROC_BROWSER_TEST_F(SpeechInputBubbleTest, ShowAndHide) {
  gfx::Rect element_rect(100, 100, 100, 100);
  scoped_ptr<SpeechInputBubble> bubble(SpeechInputBubble::Create(
      browser()->GetSelectedTabContents(), this, element_rect));
  EXPECT_TRUE(bubble.get());
  bubble->Show();
  bubble->Hide();
}

IN_PROC_BROWSER_TEST_F(SpeechInputBubbleTest, ShowAndHideTwice) {
  gfx::Rect element_rect(100, 100, 100, 100);
  scoped_ptr<SpeechInputBubble> bubble(SpeechInputBubble::Create(
      browser()->GetSelectedTabContents(), this, element_rect));
  EXPECT_TRUE(bubble.get());
  bubble->Show();
  bubble->Hide();
  bubble->Show();
  bubble->Hide();
}
