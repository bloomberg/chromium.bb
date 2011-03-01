// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/speech/speech_input_bubble_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"

class SkBitmap;

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

  MockSpeechInputBubble(TabContents* tab_contents,
                        Delegate* delegate,
                        const gfx::Rect&)
      : SpeechInputBubbleBase(tab_contents) {
    VLOG(1) << "MockSpeechInputBubble created";
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableFunction(&InvokeDelegate, delegate));
  }

  static void InvokeDelegate(Delegate* delegate) {
    VLOG(1) << "MockSpeechInputBubble invoking delegate for type " << type_;
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
  virtual void SetImage(const SkBitmap&) {}

 private:
  static BubbleType type_;
};

// The test fixture.
class SpeechInputBubbleControllerTest
    : public SpeechInputBubbleControllerDelegate,
      public BrowserWithTestWindowTest {
 public:
  SpeechInputBubbleControllerTest()
      : BrowserWithTestWindowTest(),
        io_thread_(BrowserThread::IO),  // constructs a new thread and loop
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
    VLOG(1) << "Received InfoBubbleButtonClicked for button " << button;
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (button == SpeechInputBubble::BUTTON_CANCEL) {
      cancel_clicked_ = true;
    } else if (button == SpeechInputBubble::BUTTON_TRY_AGAIN) {
      try_again_clicked_ = true;
    }
    message_loop()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  virtual void InfoBubbleFocusChanged(int caller_id) {
    VLOG(1) << "Received InfoBubbleFocusChanged";
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    focus_changed_ = true;
    message_loop()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  // testing::Test methods.
  virtual void SetUp() {
    BrowserWithTestWindowTest::SetUp();
    SpeechInputBubble::set_factory(
        &SpeechInputBubbleControllerTest::CreateBubble);
    io_thread_.Start();
  }

  virtual void TearDown() {
    SpeechInputBubble::set_factory(NULL);
    io_thread_.Stop();
    BrowserWithTestWindowTest::TearDown();
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
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    // Set up to activate the bubble soon after it gets created, since we test
    // events sent by the bubble and those are handled only when the bubble is
    // active.
    MessageLoop::current()->PostTask(FROM_HERE,
                                     NewRunnableFunction(&ActivateBubble));

    // The |tab_contents| parameter would be NULL since the dummy caller id
    // passed to CreateBubble would not have matched any active tab. So get a
    // real TabContents pointer from the test fixture and pass that, because
    // the bubble controller registers for tab close notifications which need
    // a valid TabContents.
    tab_contents = test_fixture_->browser()->GetSelectedTabContents();
    return new MockSpeechInputBubble(tab_contents, delegate, element_rect);
  }

 protected:
  // The main thread of the test is marked as the IO thread and we create a new
  // one for the UI thread.
  BrowserThread io_thread_;
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
TEST_F(SpeechInputBubbleControllerTest, TestFocusChanged) {
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
