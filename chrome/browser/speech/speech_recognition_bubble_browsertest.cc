// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/speech/speech_recognition_bubble.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"

class SpeechRecognitionBubbleTest : public SpeechRecognitionBubbleDelegate,
                              public InProcessBrowserTest {
 public:
  // SpeechRecognitionBubble::Delegate methods.
  virtual void InfoBubbleButtonClicked(
      SpeechRecognitionBubble::Button button) OVERRIDE {
  }
  virtual void InfoBubbleFocusChanged() OVERRIDE {}

 protected:
};

IN_PROC_BROWSER_TEST_F(SpeechRecognitionBubbleTest, CreateAndDestroy) {
  gfx::Rect element_rect(100, 100, 100, 100);
  scoped_ptr<SpeechRecognitionBubble> bubble(SpeechRecognitionBubble::Create(
      browser()->tab_strip_model()->GetActiveWebContents(),
      this, element_rect));
  EXPECT_TRUE(bubble.get());
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionBubbleTest, ShowAndDestroy) {
  gfx::Rect element_rect(100, 100, 100, 100);
  scoped_ptr<SpeechRecognitionBubble> bubble(SpeechRecognitionBubble::Create(
      browser()->tab_strip_model()->GetActiveWebContents(),
      this, element_rect));
  EXPECT_TRUE(bubble.get());
  bubble->Show();
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionBubbleTest, ShowAndHide) {
  gfx::Rect element_rect(100, 100, 100, 100);
  scoped_ptr<SpeechRecognitionBubble> bubble(SpeechRecognitionBubble::Create(
      browser()->tab_strip_model()->GetActiveWebContents(),
      this, element_rect));
  EXPECT_TRUE(bubble.get());
  bubble->Show();
  bubble->Hide();
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionBubbleTest, ShowAndHideTwice) {
  gfx::Rect element_rect(100, 100, 100, 100);
  scoped_ptr<SpeechRecognitionBubble> bubble(SpeechRecognitionBubble::Create(
      browser()->tab_strip_model()->GetActiveWebContents(),
      this, element_rect));
  EXPECT_TRUE(bubble.get());
  bubble->Show();
  bubble->Hide();
  bubble->Show();
  bubble->Hide();
}
