// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/speech/speech_input_bubble_controller.h"
#include "gfx/rect.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech_input {

// A mock bubble class which fakes a focus change or recognition cancel by the
// user and closing of the info bubble.
class MockSpeechInputBubble : public SpeechInputBubble {
 public:
  enum BubbleType {
    BUBBLE_TEST_FOCUS_CHANGED,
    BUBBLE_TEST_RECOGNITION_CANCELLED
  };

  MockSpeechInputBubble(TabContents*, Delegate* delegate, const gfx::Rect&) {
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableFunction(&InvokeDelegate, delegate));
  }

  static void InvokeDelegate(Delegate* delegate) {
    if (type_ == BUBBLE_TEST_FOCUS_CHANGED)
      delegate->InfoBubbleClosed();
    else
      delegate->RecognitionCancelled();
  }

  static void set_type(BubbleType type) {
    type_ = type;
  }

  virtual void SetRecognizingMode() {}

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
        recognition_cancelled_(false),
        focus_changed_(false) {
  }

  // SpeechInputBubbleControllerDelegate methods.
  virtual void RecognitionCancelled(int caller_id) {
    EXPECT_TRUE(ChromeThread::CurrentlyOn(ChromeThread::IO));
    recognition_cancelled_ = true;
    MessageLoop::current()->Quit();
  }

  virtual void SpeechInputFocusChanged(int caller_id) {
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

  static SpeechInputBubble* CreateBubble(TabContents* tab_contents,
                                         SpeechInputBubble::Delegate* delegate,
                                         const gfx::Rect& element_rect) {
    EXPECT_TRUE(ChromeThread::CurrentlyOn(ChromeThread::UI));
    return new MockSpeechInputBubble(tab_contents, delegate, element_rect);
  }

 protected:
  // The main thread of the test is marked as the IO thread and we create a new
  // one for the UI thread.
  MessageLoop io_loop_;
  ChromeThread ui_thread_;
  ChromeThread io_thread_;
  bool recognition_cancelled_;
  bool focus_changed_;
};

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

  scoped_refptr<SpeechInputBubbleController> controller(
      new SpeechInputBubbleController(this));

  controller->CreateBubble(0, 1, 1, gfx::Rect(1, 1));
  MessageLoop::current()->Run();
  EXPECT_TRUE(focus_changed_);
  EXPECT_FALSE(recognition_cancelled_);
}

// Test that the speech bubble UI gets created in the UI thread and that the
// recognition cancelled callback comes back in the IO thread.
TEST_F(SpeechInputBubbleControllerTest, TestRecognitionCancelled) {
  MockSpeechInputBubble::set_type(
      MockSpeechInputBubble::BUBBLE_TEST_RECOGNITION_CANCELLED);

  scoped_refptr<SpeechInputBubbleController> controller(
      new SpeechInputBubbleController(this));

  controller->CreateBubble(0, 1, 1, gfx::Rect(1, 1));
  MessageLoop::current()->Run();
  EXPECT_TRUE(recognition_cancelled_);
  EXPECT_FALSE(focus_changed_);
}

}  // namespace speech_input
