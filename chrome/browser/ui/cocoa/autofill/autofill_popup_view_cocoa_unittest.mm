// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_popup_view_cocoa.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/base/cocoa/touch_bar_util.h"
#include "ui/gfx/geometry/rect_f.h"

namespace {

NSString* const kCreditCardAutofillTouchBarId = @"credit-card-autofill";

}  // namespace

class MockAutofillPopupController : public autofill::AutofillPopupController {
 public:
  MockAutofillPopupController() {
    gfx::FontList::SetDefaultFontDescription("Arial, Times New Roman, 15px");
    layout_model_.reset(new autofill::AutofillPopupLayoutModel(this, false));
  }

  // AutofillPopupViewDelegate
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD0(ViewDestroyed, void());
  MOCK_METHOD1(SetSelectionAtPoint, void(const gfx::Point& point));
  MOCK_METHOD0(AcceptSelectedLine, bool());
  MOCK_METHOD0(SelectionCleared, void());
  MOCK_CONST_METHOD0(popup_bounds, gfx::Rect());
  MOCK_METHOD0(container_view, gfx::NativeView());
  MOCK_CONST_METHOD0(element_bounds, const gfx::RectF&());
  MOCK_CONST_METHOD0(IsRTL, bool());
  const std::vector<autofill::Suggestion> GetSuggestions() override {
    std::vector<autofill::Suggestion> suggestions(
        GetLineCount(), autofill::Suggestion("", "", "", 0));
    return suggestions;
  }
  MOCK_METHOD1(SetTypesetter, void(gfx::Typesetter typesetter));
  MOCK_METHOD1(GetElidedValueWidthForRow, int(int row));
  MOCK_METHOD1(GetElidedLabelWidthForRow, int(int row));

  // AutofillPopupController
  MOCK_METHOD0(OnSuggestionsChanged, void());
  MOCK_METHOD1(AcceptSuggestion, void(int index));

  int GetLineCount() const override { return line_count_; }

  const autofill::Suggestion& GetSuggestionAt(int row) const override {
    return suggestion_;
  }
  MOCK_CONST_METHOD1(GetElidedValueAt, const base::string16&(int row));
  MOCK_CONST_METHOD1(GetElidedLabelAt, const base::string16&(int row));
  MOCK_METHOD3(GetRemovalConfirmationText,
               bool(int index, base::string16* title, base::string16* body));
  MOCK_METHOD1(RemoveSuggestion, bool(int index));
  MOCK_CONST_METHOD1(GetBackgroundColorIDForRow,
                     ui::NativeTheme::ColorId(int index));
  MOCK_CONST_METHOD0(selected_line, base::Optional<int>());
  const autofill::AutofillPopupLayoutModel& layout_model() const override {
    return *layout_model_;
  }

  void SetIsCreditCardField(bool is_credit_card_field) {
    layout_model_.reset(
        new autofill::AutofillPopupLayoutModel(this, is_credit_card_field));
  }

  void set_line_count(int line_count) { line_count_ = line_count; }

 private:
  int line_count_;
  std::unique_ptr<autofill::AutofillPopupLayoutModel> layout_model_;
  autofill::Suggestion suggestion_;
};

class AutofillPopupViewCocoaUnitTest : public CocoaTest {
 public:
  void SetUp() override {
    CocoaTest::SetUp();
    feature_list.InitAndEnableFeature(autofill::kCreditCardAutofillTouchBar);

    // Ensure the strings in the model are elided with the BROWSER typesetter
    // (i.e. CoreText), since this test can only show Cocoa UI.
    EXPECT_CALL(autofill_popup_controller_,
                SetTypesetter(gfx::Typesetter::BROWSER));

    view_.reset([[AutofillPopupViewCocoa alloc]
        initWithController:&autofill_popup_controller_
                     frame:NSZeroRect
                  delegate:nil]);
  }

  void SetLineCount(int line_count) {
    autofill_popup_controller_.set_line_count(line_count);
  }

  // Used to enable the the browser window touch bar.
  base::test::ScopedFeatureList feature_list;

  base::scoped_nsobject<AutofillPopupViewCocoa> view_;
  MockAutofillPopupController autofill_popup_controller_;
};

// Tests to check if the touch bar shows up properly.
TEST_F(AutofillPopupViewCocoaUnitTest, CreditCardAutofillTouchBar) {
  if (@available(macOS 10.12.2, *)) {
    // Touch bar shouldn't appear if the popup is not for credit cards.
    autofill_popup_controller_.SetIsCreditCardField(false);
    EXPECT_FALSE([view_ makeTouchBar]);

    // Touch bar shouldn't appear if the popup is empty.
    autofill_popup_controller_.SetIsCreditCardField(true);
    SetLineCount(0);
    EXPECT_FALSE([view_ makeTouchBar]);

    autofill_popup_controller_.SetIsCreditCardField(true);
    SetLineCount(3);
    NSTouchBar* touch_bar = [view_ makeTouchBar];
    EXPECT_TRUE(touch_bar);
    EXPECT_TRUE([[touch_bar customizationIdentifier]
        isEqual:ui::GetTouchBarId(kCreditCardAutofillTouchBarId)]);
  }
}

// Tests that the touch bar histogram is logged correctly.
TEST_F(AutofillPopupViewCocoaUnitTest, CreditCardAutofillTouchBarMetric) {
  {
    base::HistogramTester histogram_tester;
    [view_ acceptCreditCard:nil];
    histogram_tester.ExpectBucketCount("TouchBar.Default.Metrics",
                                       ui::TouchBarAction::CREDIT_CARD_AUTOFILL,
                                       1);
  }
}
