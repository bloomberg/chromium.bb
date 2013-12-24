// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/generated_credit_card_bubble_controller.h"
#include "chrome/browser/ui/autofill/test_generated_credit_card_bubble_controller.h"
#include "chrome/browser/ui/autofill/test_generated_credit_card_bubble_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/range/range.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

namespace autofill {

namespace {

base::string16 BackingCard() {
  return base::ASCIIToUTF16("Visa - 1111");
}

base::string16 FrontingCard() {
  return base::ASCIIToUTF16("Mastercard - 4444");
}

base::string16 RangeOfString(const base::string16& string,
                             const gfx::Range& range) {
  return string.substr(range.start(), range.end() - range.start());
}

class GeneratedCreditCardBubbleControllerTest : public testing::Test {
 public:
  GeneratedCreditCardBubbleControllerTest()
      : test_web_contents_(
            content::WebContentsTester::CreateTestWebContents(
                &profile_, NULL)) {}

  virtual void SetUp() OVERRIDE {
    // Attaches immediately to |test_web_contents_| so a test version will exist
    // before a non-test version can be created.
    new TestGeneratedCreditCardBubbleController(test_web_contents_.get());
    ASSERT_TRUE(controller()->IsInstalled());
  }

 protected:
  TestGeneratedCreditCardBubbleController* controller() {
    return static_cast<TestGeneratedCreditCardBubbleController*>(
        TestGeneratedCreditCardBubbleController::FromWebContents(
            test_web_contents_.get()));
  }

  int GeneratedCardBubbleTimesShown() {
    return profile_.GetPrefs()->GetInteger(
        ::prefs::kAutofillGeneratedCardBubbleTimesShown);
  }

  void Show() {
    ASSERT_TRUE(controller()->IsInstalled());
    TestGeneratedCreditCardBubbleController::Show(test_web_contents_.get(),
                                                  FrontingCard(),
                                                  BackingCard());
  }

  void NavigateWithTransition(content::PageTransition trans) {
    content::WebContentsTester::For(test_web_contents_.get())->TestDidNavigate(
        test_web_contents_->GetRenderViewHost(), 1, GURL("about:blank"), trans);
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
#if defined(OS_WIN)
  // Without this there will be drag and drop failures. http://crbug.com/227221
  ui::ScopedOleInitializer ole_initializer_;
#endif
  TestingProfile profile_;
  scoped_ptr<content::WebContents> test_web_contents_;
};

}  // namespace

TEST_F(GeneratedCreditCardBubbleControllerTest, GeneratedCardBubbleTimesShown) {
  ASSERT_EQ(0, GeneratedCardBubbleTimesShown());

  // Ensure that showing the generated card UI bumps the persistent count.
  Show();
  EXPECT_EQ(1, GeneratedCardBubbleTimesShown());
  EXPECT_TRUE(controller()->GetTestingBubble()->showing());

  Show();
  Show();
  EXPECT_EQ(3, GeneratedCardBubbleTimesShown());
  EXPECT_TRUE(controller()->GetTestingBubble()->showing());
}

TEST_F(GeneratedCreditCardBubbleControllerTest, TitleText) {
  Show();
  EXPECT_FALSE(controller()->TitleText().empty());
}

TEST_F(GeneratedCreditCardBubbleControllerTest, ContentsText) {
  // Ensure that while showing the generated card UI that the bubble's text
  // contains "Visa - 1111" and "Mastercard - 4444".
  Show();
  base::string16 contents_text = controller()->ContentsText();
  EXPECT_NE(base::string16::npos, contents_text.find(BackingCard()));
  EXPECT_NE(base::string16::npos, contents_text.find(FrontingCard()));

  // Make sure that |bubble_text_| is regenerated the same way in |Setup()|.
  Show();
  EXPECT_EQ(contents_text, controller()->ContentsText());
}

TEST_F(GeneratedCreditCardBubbleControllerTest, ContentsTextRanges) {
  // Check that the highlighted ranges in the bubble's text are correct.
  Show();
  const base::string16& contents_text = controller()->ContentsText();
  const std::vector<TextRange>& ranges = controller()->ContentsTextRanges();

  ASSERT_EQ(3U, ranges.size());

  EXPECT_EQ(FrontingCard(), RangeOfString(contents_text, ranges[0].range));
  EXPECT_FALSE(ranges[0].is_link);

  EXPECT_EQ(BackingCard(), RangeOfString(contents_text, ranges[1].range));
  EXPECT_FALSE(ranges[1].is_link);

  EXPECT_TRUE(ranges[2].is_link);

  Show();
  EXPECT_EQ(ranges, controller()->ContentsTextRanges());
}

TEST_F(GeneratedCreditCardBubbleControllerTest, AnchorIcon) {
  Show();
  EXPECT_FALSE(controller()->AnchorIcon().IsEmpty());
}

TEST_F(GeneratedCreditCardBubbleControllerTest, HideOnLinkClick) {
  EXPECT_FALSE(controller()->GetTestingBubble());
  Show();
  EXPECT_TRUE(controller()->GetTestingBubble()->showing());

  // However, if the user clicks a link the bubble should hide.
  NavigateWithTransition(content::PAGE_TRANSITION_LINK);
  EXPECT_FALSE(controller());
}

TEST_F(GeneratedCreditCardBubbleControllerTest, StayOnSomeNavigations) {
  EXPECT_FALSE(controller()->GetTestingBubble());
  Show();
  EXPECT_TRUE(controller()->GetTestingBubble()->showing());

  // If the user reloads or the page redirects or submits a form, the bubble
  // should stay showing.
  NavigateWithTransition(content::PAGE_TRANSITION_CLIENT_REDIRECT);
  NavigateWithTransition(content::PAGE_TRANSITION_FORM_SUBMIT);
  NavigateWithTransition(content::PAGE_TRANSITION_RELOAD);
  EXPECT_TRUE(controller()->GetTestingBubble()->showing());
}

}  // namespace autofill
