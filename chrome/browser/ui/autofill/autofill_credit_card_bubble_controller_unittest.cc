// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_credit_card_bubble_controller.h"
#include "chrome/browser/ui/autofill/test_autofill_credit_card_bubble.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/range/range.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

namespace autofill {

namespace {

base::string16 BackingCard() {
  return ASCIIToUTF16("Visa - 1111");
}
base::string16 FrontingCard() {
  return ASCIIToUTF16("Mastercard - 4444");
}
base::string16 NewCard() {
  return ASCIIToUTF16("Discover - 7777");
}

base::string16 RangeOfString(const base::string16& string,
                             const ui::Range& range) {
  return string.substr(range.start(), range.end() - range.start());
}

class TestAutofillCreditCardBubbleController
    : public AutofillCreditCardBubbleController {
 public:
  explicit TestAutofillCreditCardBubbleController(
      content::WebContents* contents)
      : AutofillCreditCardBubbleController(contents) {
    contents->SetUserData(UserDataKey(), this);
  }

  virtual ~TestAutofillCreditCardBubbleController() {}

  bool IsInstalled() const {
    return web_contents()->GetUserData(UserDataKey()) == this;
  }

  TestAutofillCreditCardBubble* GetTestingBubble() {
    return static_cast<TestAutofillCreditCardBubble*>(
        AutofillCreditCardBubbleController::bubble().get());
  }

 protected:
  virtual base::WeakPtr<AutofillCreditCardBubble> CreateBubble() OVERRIDE {
    return TestAutofillCreditCardBubble::Create(GetWeakPtr());
  }

  virtual bool CanShow() const OVERRIDE {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAutofillCreditCardBubbleController);
};

class AutofillCreditCardBubbleControllerTest : public testing::Test {
 public:
  AutofillCreditCardBubbleControllerTest()
      : test_web_contents_(
            content::WebContentsTester::CreateTestWebContents(
                profile(), NULL)) {}

  virtual void SetUp() OVERRIDE {
    // Attaches immediately to |test_web_contents_| so a test version will exist
    // before a non-test version can be created.
    new TestAutofillCreditCardBubbleController(test_web_contents_.get());
    ASSERT_TRUE(controller()->IsInstalled());
  }

 protected:
  TestAutofillCreditCardBubbleController* controller() {
    return static_cast<TestAutofillCreditCardBubbleController*>(
        TestAutofillCreditCardBubbleController::FromWebContents(
            test_web_contents_.get()));
  }

  int GeneratedCardBubbleTimesShown() {
    return profile()->GetPrefs()->GetInteger(
        ::prefs::kAutofillGeneratedCardBubbleTimesShown);
  }

  Profile* profile() { return &profile_; }

  content::WebContentsTester* test_web_contents() {
    return content::WebContentsTester::For(test_web_contents_.get());
  }

  void ShowGeneratedCardUI() {
    ASSERT_TRUE(controller()->IsInstalled());
    TestAutofillCreditCardBubbleController::ShowGeneratedCardUI(
        test_web_contents_.get(), BackingCard(), FrontingCard());
  }

  void ShowNewCardSavedBubble() {
    ASSERT_TRUE(controller()->IsInstalled());
    TestAutofillCreditCardBubbleController::ShowNewCardSavedBubble(
        test_web_contents_.get(), NewCard());
  }

  void Navigate() {
    NavigateWithTransition(content::PAGE_TRANSITION_LINK);
  }

  void Redirect() {
    NavigateWithTransition(content::PAGE_TRANSITION_CLIENT_REDIRECT);
  }

 private:
  void NavigateWithTransition(content::PageTransition trans) {
    test_web_contents()->TestDidNavigate(
        test_web_contents_->GetRenderViewHost(), 1, GURL("about:blank"), trans);
  }

  content::TestBrowserThreadBundle thread_bundle_;
#if defined(OS_WIN)
  // Without this there will be drag and drop failures. http://crbug.com/227221
  ui::ScopedOleInitializer ole_initializer_;
#endif
  TestingProfile profile_;
  scoped_ptr<content::WebContents> test_web_contents_;
};

}  // namespace

TEST_F(AutofillCreditCardBubbleControllerTest, GeneratedCardBubbleTimesShown) {
  ASSERT_EQ(0, GeneratedCardBubbleTimesShown());

  // Ensure that showing the generated card UI bumps the persistent count.
  ShowGeneratedCardUI();
  EXPECT_EQ(1, GeneratedCardBubbleTimesShown());
  EXPECT_TRUE(controller()->GetTestingBubble()->showing());

  ShowGeneratedCardUI();
  ShowGeneratedCardUI();
  EXPECT_EQ(3, GeneratedCardBubbleTimesShown());
  EXPECT_TRUE(controller()->GetTestingBubble()->showing());
}

TEST_F(AutofillCreditCardBubbleControllerTest, BubbleText) {
  // Ensure that while showing the generated card UI that the bubble's text
  // contains "Visa - 1111" and "Mastercard - 4444".
  ShowGeneratedCardUI();
  base::string16 generated_text = controller()->BubbleText();
  EXPECT_NE(base::string16::npos, generated_text.find(BackingCard()));
  EXPECT_NE(base::string16::npos, generated_text.find(FrontingCard()));
  EXPECT_EQ(base::string16::npos, generated_text.find(NewCard()));

  // Ensure that while showing the new card bubble that "Discover - 7777" is in
  // the bubble text.
  ShowNewCardSavedBubble();
  base::string16 new_text = controller()->BubbleText();
  EXPECT_NE(new_text, generated_text);
  EXPECT_EQ(base::string16::npos, new_text.find(BackingCard()));
  EXPECT_EQ(base::string16::npos, new_text.find(FrontingCard()));
  EXPECT_NE(base::string16::npos, new_text.find(NewCard()));

  // Make sure that |bubble_text_| is regenerated the same way in |Setup()|.
  ShowGeneratedCardUI();
  EXPECT_EQ(generated_text, controller()->BubbleText());

  ShowNewCardSavedBubble();
  EXPECT_EQ(new_text, controller()->BubbleText());
}

TEST_F(AutofillCreditCardBubbleControllerTest, BubbleTextRanges) {
  // Check that the highlighted ranges in the bubble's text are correct.
  ShowGeneratedCardUI();
  base::string16 text = controller()->BubbleText();
  std::vector<ui::Range> ranges = controller()->BubbleTextRanges();

  ASSERT_EQ(2U, ranges.size());
  EXPECT_EQ(BackingCard(), RangeOfString(text, ranges[0]));
  EXPECT_EQ(FrontingCard(), RangeOfString(text, ranges[1]));

  ShowNewCardSavedBubble();
  text = controller()->BubbleText();
  ranges = controller()->BubbleTextRanges();

  ASSERT_EQ(1U, ranges.size());
  EXPECT_EQ(NewCard(), RangeOfString(text, ranges[0]));
}

TEST_F(AutofillCreditCardBubbleControllerTest, HideOnNavigate) {
  // When a user navigates away from a page (or refreshes) normally, the bubbles
  // should be hidden.
  EXPECT_FALSE(controller()->GetTestingBubble());
  ShowGeneratedCardUI();
  EXPECT_TRUE(controller()->GetTestingBubble()->showing());

  Navigate();
  EXPECT_FALSE(controller());

  SetUp();

  EXPECT_FALSE(controller()->GetTestingBubble());
  ShowNewCardSavedBubble();
  EXPECT_TRUE(controller()->GetTestingBubble()->showing());

  Navigate();
  EXPECT_FALSE(controller());
}

TEST_F(AutofillCreditCardBubbleControllerTest, StayOnRedirect) {
  // If a page redirects right after submitting, the bubble should remain.
  EXPECT_FALSE(controller()->GetTestingBubble());
  ShowGeneratedCardUI();
  EXPECT_TRUE(controller()->GetTestingBubble()->showing());

  Redirect();
  EXPECT_TRUE(controller()->GetTestingBubble()->showing());

  SetUp();

  EXPECT_FALSE(controller()->GetTestingBubble());
  ShowNewCardSavedBubble();
  EXPECT_TRUE(controller()->GetTestingBubble()->showing());

  Redirect();
  EXPECT_TRUE(controller()->GetTestingBubble()->showing());
}

}  // namespace autofill
