// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/speech/speech_input_bubble_controller.h"
#include "gfx/rect.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech_input {

// A mock bubble class which fakes a focus change or recognition cancel by the
// user and closing of the info bubble.
class MockSpeechInputBubble : public SpeechInputBubbleBase {
 public:
  enum BubbleType {
    BUBBLE_TEST_FOCUS_CHANGED,
    BUBBLE_TEST_CLICK_CANCEL,
    BUBBLE_TEST_CLICK_TRY_AGAIN,
  };

  MockSpeechInputBubble(TabContents*, Delegate* delegate, const gfx::Rect&) {
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableFunction(&InvokeDelegate, delegate));
  }

  static void InvokeDelegate(Delegate* delegate) {
    switch (type_) {
      case BUBBLE_TEST_FOCUS_CHANGED:
        delegate->InfoBubbleFocusChanged();
        break;
      case BUBBLE_TEST_CLICK_CANCEL:
        delegate->InfoBubbleButtonClicked(SpeechInputBubble::BUTTON_CANCEL);
        break;
      case BUBBLE_TEST_CLICK_TRY_AGAIN:
        delegate->InfoBubbleButtonClicked(SpeechInputBubble::BUTTON_TRY_AGAIN);
        break;
    }
  }

  static void set_type(BubbleType type) {
    type_ = type;
  }
  static BubbleType type() {
    return type_;
  }

  virtual void Show() {}
  virtual void Hide() {}
  virtual void UpdateLayout() {}

 private:
  static BubbleType type_;
};

// The test fixture.
class SpeechInputBubbleControllerTest
    : public SpeechInputBubbleControllerDelegate,
      public testing::Test {
 public:
  SpeechInputBubbleControllerTest()
      : io_loop_(MessageLoop::TYPE_IO),
        ui_thread_(ChromeThread::UI),  // constructs a new thread and loop
        io_thread_(ChromeThread::IO, &io_loop_),  // resuses main thread loop
        cancel_clicked_(false),
        try_again_clicked_(false),
        focus_changed_(false),
        controller_(ALLOW_THIS_IN_INITIALIZER_LIST(
            new SpeechInputBubbleController(this))) {
    EXPECT_EQ(NULL, test_fixture_);
    test_fixture_ = this;
  }

  ~SpeechInputBubbleControllerTest() {
    test_fixture_ = NULL;
  }

  // SpeechInputBubbleControllerDelegate methods.
  virtual void InfoBubbleButtonClicked(int caller_id,
                                       SpeechInputBubble::Button button) {
    EXPECT_TRUE(ChromeThread::CurrentlyOn(ChromeThread::IO));
    if (button == SpeechInputBubble::BUTTON_CANCEL) {
      cancel_clicked_ = true;
    } else if (button == SpeechInputBubble::BUTTON_TRY_AGAIN) {
      try_again_clicked_ = true;
    }
    MessageLoop::current()->Quit();
  }

  virtual void InfoBubbleFocusChanged(int caller_id) {
    EXPECT_TRUE(ChromeThread::CurrentlyOn(ChromeThread::IO));
    focus_changed_ = true;
    MessageLoop::current()->Quit();
  }

  // testing::Test methods.
  virtual void SetUp() {
    SpeechInputBubble::set_factory(
        &SpeechInputBubbleControllerTest::CreateBubble);
    ui_thread_.Start();
  }

  virtual void TearDown() {
    SpeechInputBubble::set_factory(NULL);
    ui_thread_.Stop();
  }

  static void ActivateBubble() {
    if (MockSpeechInputBubble::type() ==
        MockSpeechInputBubble::BUBBLE_TEST_FOCUS_CHANGED) {
      test_fixture_->controller_->SetBubbleRecordingMode(kBubbleCallerId);
    } else {
      test_fixture_->controller_->SetBubbleMessage(kBubbleCallerId,
                                                   ASCIIToUTF16("Test"));
    }
  }

  static SpeechInputBubble* CreateBubble(TabContents* tab_contents,
                                         SpeechInputBubble::Delegate* delegate,
                                         const gfx::Rect& element_rect) {
    EXPECT_TRUE(ChromeThread::CurrentlyOn(ChromeThread::UI));
    // Set up to activate the bubble soon after it gets created, since we test
    // events sent by the bubble and those are handled only when the bubble is
    // active.
    MessageLoop::current()->PostTask(FROM_HERE,
                                     NewRunnableFunction(&ActivateBubble));
    return new MockSpeechInputBubble(tab_contents, delegate, element_rect);
  }

 protected:
  // The main thread of the test is marked as the IO thread and we create a new
  // one for the UI thread.
  MessageLoop io_loop_;
  ChromeThread ui_thread_;
  ChromeThread io_thread_;
  bool cancel_clicked_;
  bool try_again_clicked_;
  bool focus_changed_;
  scoped_refptr<SpeechInputBubbleController> controller_;

  static const int kBubbleCallerId;
  static SpeechInputBubbleControllerTest* test_fixture_;
};

SpeechInputBubbleControllerTest*
SpeechInputBubbleControllerTest::test_fixture_ = NULL;

const int SpeechInputBubbleControllerTest::kBubbleCallerId = 1;

MockSpeechInputBubble::BubbleType MockSpeechInputBubble::type_ =
    MockSpeechInputBubble::BUBBLE_TEST_FOCUS_CHANGED;

// Test that the speech bubble UI gets created in the UI thread and that the
// focus changed callback comes back in the IO thread.
//
// Crashes on Win only.  http://crbug.com/54044
#if defined(OS_WIN)
#define MAYBE_TestFocusChanged DISABLED_TestFocusChanged
#else
#define MAYBE_TestFocusChanged TestFocusChanged
#endif
TEST_F(SpeechInputBubbleControllerTest, MAYBE_TestFocusChanged) {
  MockSpeechInputBubble::set_type(
      MockSpeechInputBubble::BUBBLE_TEST_FOCUS_CHANGED);

  controller_->CreateBubble(kBubbleCallerId, 1, 1, gfx::Rect(1, 1));
  MessageLoop::current()->Run();
  EXPECT_TRUE(focus_changed_);
  EXPECT_FALSE(cancel_clicked_);
  EXPECT_FALSE(try_again_clicked_);
  controller_->CloseBubble(kBubbleCallerId);
}

// Test that the speech bubble UI gets created in the UI thread and that the
// recognition cancelled callback comes back in the IO thread.
TEST_F(SpeechInputBubbleControllerTest, TestRecognitionCancelled) {
  MockSpeechInputBubble::set_type(
      MockSpeechInputBubble::BUBBLE_TEST_CLICK_CANCEL);

  controller_->CreateBubble(kBubbleCallerId, 1, 1, gfx::Rect(1, 1));
  MessageLoop::current()->Run();
  EXPECT_TRUE(cancel_clicked_);
  EXPECT_FALSE(try_again_clicked_);
  EXPECT_FALSE(focus_changed_);
  controller_->CloseBubble(kBubbleCallerId);
}

// Test that the speech bubble UI gets created in the UI thread and that the
// try-again button click event comes back in the IO thread.
TEST_F(SpeechInputBubbleControllerTest, TestTryAgainClicked) {
  MockSpeechInputBubble::set_type(
      MockSpeechInputBubble::BUBBLE_TEST_CLICK_TRY_AGAIN);

  controller_->CreateBubble(kBubbleCallerId, 1, 1, gfx::Rect(1, 1));
  MessageLoop::current()->Run();
  EXPECT_FALSE(cancel_clicked_);
  EXPECT_TRUE(try_again_clicked_);
  EXPECT_FALSE(focus_changed_);
  controller_->CloseBubble(kBubbleCallerId);
}

}  // namespace speech_input
