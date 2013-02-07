// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/speech/speech_recognition_bubble_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"

using content::BrowserThread;
using content::WebContents;

namespace speech {

// A mock bubble class which fakes a focus change or recognition cancel by the
// user and closing of the info bubble.
class MockSpeechRecognitionBubble : public SpeechRecognitionBubbleBase {
 public:
  enum BubbleType {
    BUBBLE_TEST_FOCUS_CHANGED,
    BUBBLE_TEST_CLICK_CANCEL,
    BUBBLE_TEST_CLICK_TRY_AGAIN,
  };

  MockSpeechRecognitionBubble(WebContents* web_contents,
                        Delegate* delegate,
                        const gfx::Rect&)
      : SpeechRecognitionBubbleBase(web_contents) {
    VLOG(1) << "MockSpeechRecognitionBubble created";
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&InvokeDelegate, delegate));
  }

  static void InvokeDelegate(Delegate* delegate) {
    VLOG(1) << "MockSpeechRecognitionBubble invoking delegate for type "
            << type_;
    switch (type_) {
      case BUBBLE_TEST_FOCUS_CHANGED:
        delegate->InfoBubbleFocusChanged();
        break;
      case BUBBLE_TEST_CLICK_CANCEL:
        delegate->InfoBubbleButtonClicked(
            SpeechRecognitionBubble::BUTTON_CANCEL);
        break;
      case BUBBLE_TEST_CLICK_TRY_AGAIN:
        delegate->InfoBubbleButtonClicked(
            SpeechRecognitionBubble::BUTTON_TRY_AGAIN);
        break;
    }
  }

  static void set_type(BubbleType type) {
    type_ = type;
  }
  static BubbleType type() {
    return type_;
  }

  virtual void Show() OVERRIDE {}
  virtual void Hide() OVERRIDE {}
  virtual void UpdateLayout() OVERRIDE {}
  virtual void UpdateImage() OVERRIDE {}

 private:
  static BubbleType type_;
};

// The test fixture.
class SpeechRecognitionBubbleControllerTest
    : public SpeechRecognitionBubbleControllerDelegate,
      public BrowserWithTestWindowTest {
 public:
  SpeechRecognitionBubbleControllerTest()
      : BrowserWithTestWindowTest(),
        io_thread_(BrowserThread::IO),  // constructs a new thread and loop
        cancel_clicked_(false),
        try_again_clicked_(false),
        focus_changed_(false),
        controller_(ALLOW_THIS_IN_INITIALIZER_LIST(
            new SpeechRecognitionBubbleController(this))) {
    EXPECT_EQ(NULL, test_fixture_);
    test_fixture_ = this;
  }

  virtual ~SpeechRecognitionBubbleControllerTest() {
    test_fixture_ = NULL;
  }

  // SpeechRecognitionBubbleControllerDelegate methods.
  virtual void InfoBubbleButtonClicked(
      int session_id,
      SpeechRecognitionBubble::Button button) OVERRIDE {
    VLOG(1) << "Received InfoBubbleButtonClicked for button " << button;
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (button == SpeechRecognitionBubble::BUTTON_CANCEL) {
      cancel_clicked_ = true;
    } else if (button == SpeechRecognitionBubble::BUTTON_TRY_AGAIN) {
      try_again_clicked_ = true;
    }
    message_loop()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  virtual void InfoBubbleFocusChanged(int session_id) OVERRIDE {
    VLOG(1) << "Received InfoBubbleFocusChanged";
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    focus_changed_ = true;
    message_loop()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  // testing::Test methods.
  virtual void SetUp() {
    BrowserWithTestWindowTest::SetUp();
    SpeechRecognitionBubble::set_factory(
        &SpeechRecognitionBubbleControllerTest::CreateBubble);
    io_thread_.Start();
  }

  virtual void TearDown() {
    SpeechRecognitionBubble::set_factory(NULL);
    io_thread_.Stop();
    BrowserWithTestWindowTest::TearDown();
  }

  static void ActivateBubble() {
    if (MockSpeechRecognitionBubble::type() !=
        MockSpeechRecognitionBubble::BUBBLE_TEST_FOCUS_CHANGED) {
      test_fixture_->controller_->SetBubbleMessage(ASCIIToUTF16("Test"));
    }
  }

  static SpeechRecognitionBubble* CreateBubble(
      WebContents* web_contents,
      SpeechRecognitionBubble::Delegate* delegate,
      const gfx::Rect& element_rect) {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    // Set up to activate the bubble soon after it gets created, since we test
    // events sent by the bubble and those are handled only when the bubble is
    // active.
    MessageLoop::current()->PostTask(FROM_HERE,
                                     base::Bind(&ActivateBubble));

    // The |web_contents| parameter would be NULL since the dummy session id
    // passed to CreateBubble would not have matched any active tab. So get a
    // real WebContents pointer from the test fixture and pass that, because
    // the bubble controller registers for tab close notifications which need
    // a valid WebContents.
    web_contents =
        test_fixture_->browser()->tab_strip_model()->GetActiveWebContents();
    return new MockSpeechRecognitionBubble(web_contents, delegate,
                                           element_rect);
  }

 protected:
  // The main thread of the test is marked as the IO thread and we create a new
  // one for the UI thread.
  content::TestBrowserThread io_thread_;
  bool cancel_clicked_;
  bool try_again_clicked_;
  bool focus_changed_;
  scoped_refptr<SpeechRecognitionBubbleController> controller_;

  static const int kBubbleSessionId;
  static SpeechRecognitionBubbleControllerTest* test_fixture_;
};

SpeechRecognitionBubbleControllerTest*
SpeechRecognitionBubbleControllerTest::test_fixture_ = NULL;

const int SpeechRecognitionBubbleControllerTest::kBubbleSessionId = 1;

MockSpeechRecognitionBubble::BubbleType MockSpeechRecognitionBubble::type_ =
    MockSpeechRecognitionBubble::BUBBLE_TEST_FOCUS_CHANGED;

// Test that the speech bubble UI gets created in the UI thread and that the
// focus changed callback comes back in the IO thread.
TEST_F(SpeechRecognitionBubbleControllerTest, TestFocusChanged) {
  MockSpeechRecognitionBubble::set_type(
      MockSpeechRecognitionBubble::BUBBLE_TEST_FOCUS_CHANGED);

  controller_->CreateBubble(kBubbleSessionId, 1, 1, gfx::Rect(1, 1));
  MessageLoop::current()->Run();
  EXPECT_TRUE(focus_changed_);
  EXPECT_FALSE(cancel_clicked_);
  EXPECT_FALSE(try_again_clicked_);
  controller_->CloseBubble();
}

// Test that the speech bubble UI gets created in the UI thread and that the
// recognition cancelled callback comes back in the IO thread.
TEST_F(SpeechRecognitionBubbleControllerTest, TestRecognitionCancelled) {
  MockSpeechRecognitionBubble::set_type(
      MockSpeechRecognitionBubble::BUBBLE_TEST_CLICK_CANCEL);

  controller_->CreateBubble(kBubbleSessionId, 1, 1, gfx::Rect(1, 1));
  MessageLoop::current()->Run();
  EXPECT_TRUE(cancel_clicked_);
  EXPECT_FALSE(try_again_clicked_);
  EXPECT_FALSE(focus_changed_);
  controller_->CloseBubble();
}

// Test that the speech bubble UI gets created in the UI thread and that the
// try-again button click event comes back in the IO thread.
TEST_F(SpeechRecognitionBubbleControllerTest, TestTryAgainClicked) {
  MockSpeechRecognitionBubble::set_type(
      MockSpeechRecognitionBubble::BUBBLE_TEST_CLICK_TRY_AGAIN);

  controller_->CreateBubble(kBubbleSessionId, 1, 1, gfx::Rect(1, 1));
  MessageLoop::current()->Run();
  EXPECT_FALSE(cancel_clicked_);
  EXPECT_TRUE(try_again_clicked_);
  EXPECT_FALSE(focus_changed_);
  controller_->CloseBubble();
}

}  // namespace speech
