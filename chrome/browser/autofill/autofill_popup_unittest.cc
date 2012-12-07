// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_popup_view.h"
#include "chrome/browser/autofill/test_autofill_external_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAutofillClient.h"
#include "ui/gfx/rect.h"

using ::testing::_;
using ::testing::AtLeast;
using WebKit::WebAutofillClient;

namespace {

class MockAutofillExternalDelegate :
      public autofill::TestAutofillExternalDelegate {
 public:
  MockAutofillExternalDelegate() : TestAutofillExternalDelegate(NULL, NULL) {};
  virtual ~MockAutofillExternalDelegate() {};

  virtual void SelectAutofillSuggestionAtIndex(int unique_id)
      OVERRIDE {}
  virtual void RemoveAutocompleteEntry(const string16& value) OVERRIDE {}
  virtual void RemoveAutofillProfileOrCreditCard(int unique_id) OVERRIDE {}
  virtual void ClearPreviewedForm() OVERRIDE {}

  MOCK_METHOD0(HideAutofillPopupInternal, void());
};

class TestAutofillPopupView : public AutofillPopupView {
 public:
  explicit TestAutofillPopupView(AutofillExternalDelegate* external_delegate) :
      AutofillPopupView(NULL, external_delegate, gfx::Rect()) {}
  virtual ~TestAutofillPopupView() {}

  // Making protected functions public for testing
  const std::vector<string16>& autofill_values() const {
    return AutofillPopupView::autofill_values();
  }
  int selected_line() const {
    return AutofillPopupView::selected_line();
  }
  void SetSelectedLine(size_t selected_line) {
    AutofillPopupView::SetSelectedLine(selected_line);
  }
  void SelectNextLine() {
    AutofillPopupView::SelectNextLine();
  }
  void SelectPreviousLine() {
    AutofillPopupView::SelectPreviousLine();
  }
  bool RemoveSelectedLine() {
    return AutofillPopupView::RemoveSelectedLine();
  }

  MOCK_METHOD1(InvalidateRow, void(size_t));
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD0(UpdateBoundsAndRedrawPopupInternal, void());

 private:
  virtual void ShowInternal() OVERRIDE {}
};

}  // namespace

class AutofillPopupViewUnitTest : public ::testing::Test {
 public:
  AutofillPopupViewUnitTest() {
    autofill_popup_view_.reset(new TestAutofillPopupView(&external_delegate_));
  }
  virtual ~AutofillPopupViewUnitTest() {}

protected:
  scoped_ptr<TestAutofillPopupView> autofill_popup_view_;
  MockAutofillExternalDelegate external_delegate_;
};

TEST_F(AutofillPopupViewUnitTest, SetBounds) {
  // Ensure the popup size can be set and causes a redraw.
  gfx::Rect popup_bounds(10, 10, 100, 100);

  EXPECT_CALL(*autofill_popup_view_, UpdateBoundsAndRedrawPopupInternal());

  autofill_popup_view_->SetPopupBounds(popup_bounds);

  EXPECT_EQ(popup_bounds, autofill_popup_view_->popup_bounds());
}

TEST_F(AutofillPopupViewUnitTest, ChangeSelectedLine) {
  // Set up the popup.
  std::vector<string16> autofill_values(2, string16());
  std::vector<int> autofill_ids(2, 0);
  autofill_popup_view_->Show(autofill_values, autofill_values, autofill_values,
                             autofill_ids);

  // To remove warnings.
  EXPECT_CALL(*autofill_popup_view_, InvalidateRow(_)).Times(AtLeast(0));

  EXPECT_LT(autofill_popup_view_->selected_line(), 0);
  // Check that there are at least 2 values so that the first and last selection
  // are different.
  EXPECT_GE(2,
            static_cast<int>(autofill_popup_view_->autofill_values().size()));

  // Test wrapping before the front.
  autofill_popup_view_->SelectPreviousLine();
  EXPECT_EQ(
      static_cast<int>(autofill_popup_view_->autofill_values().size() - 1),
      autofill_popup_view_->selected_line());

  // Test wrapping after the end.
  autofill_popup_view_->SelectNextLine();
  EXPECT_EQ(0, autofill_popup_view_->selected_line());
}

TEST_F(AutofillPopupViewUnitTest, RedrawSelectedLine) {
  // Set up the popup.
  std::vector<string16> autofill_values(2, string16());
  std::vector<int> autofill_ids(2, 0);
  autofill_popup_view_->Show(autofill_values, autofill_values, autofill_values,
                             autofill_ids);

  // Make sure that when a new line is selected, it is invalidated so it can
  // be updated to show it is selected.
  int selected_line = 0;
  EXPECT_CALL(*autofill_popup_view_, InvalidateRow(selected_line));
  autofill_popup_view_->SetSelectedLine(selected_line);

  // Ensure that the row isn't invalidated if it didn't change.
  EXPECT_CALL(*autofill_popup_view_, InvalidateRow(selected_line)).Times(0);
  autofill_popup_view_->SetSelectedLine(selected_line);

  // Change back to no selection.
  EXPECT_CALL(*autofill_popup_view_, InvalidateRow(selected_line));
  autofill_popup_view_->SetSelectedLine(-1);
}

TEST_F(AutofillPopupViewUnitTest, RemoveLine) {
  // Set up the popup.
  std::vector<string16> autofill_values(3, string16());
  std::vector<int> autofill_ids;
  autofill_ids.push_back(1);
  autofill_ids.push_back(1);
  autofill_ids.push_back(WebAutofillClient::MenuItemIDAutofillOptions);
  autofill_popup_view_->Show(autofill_values, autofill_values, autofill_values,
                             autofill_ids);

  // Generate a popup, so it can be hidden later. It doesn't matter what the
  // external_delegate thinks is being shown in the process, since we are just
  // testing the popup here.
  autofill::GenerateTestAutofillPopup(&external_delegate_);

  // To remove warnings.
  EXPECT_CALL(*autofill_popup_view_, InvalidateRow(_)).Times(AtLeast(0));

  // No line is selected so the removal should fail.
  EXPECT_FALSE(autofill_popup_view_->RemoveSelectedLine());

  // Try to remove the last entry and ensure it fails (it is an option).
  autofill_popup_view_->SetSelectedLine(
      autofill_popup_view_->autofill_values().size() - 1);
  EXPECT_FALSE(autofill_popup_view_->RemoveSelectedLine());
  EXPECT_LE(0, autofill_popup_view_->selected_line());

  // Remove the first entry. The popup should be redrawn since its size has
  // changed.
  EXPECT_CALL(*autofill_popup_view_, UpdateBoundsAndRedrawPopupInternal());
  autofill_popup_view_->SetSelectedLine(0);
  EXPECT_TRUE(autofill_popup_view_->RemoveSelectedLine());

  // Remove the last entry. The popup should then be hidden since there are
  // no Autofill entries left.
  EXPECT_CALL(external_delegate_, HideAutofillPopupInternal());

  autofill_popup_view_->SetSelectedLine(0);
  EXPECT_TRUE(autofill_popup_view_->RemoveSelectedLine());
}

TEST_F(AutofillPopupViewUnitTest, SkipSeparator) {
  // Set up the popup.
  std::vector<string16> autofill_values(3, string16());
  std::vector<int> autofill_ids;
  autofill_ids.push_back(1);
  autofill_ids.push_back(WebAutofillClient::MenuItemIDSeparator);
  autofill_ids.push_back(WebAutofillClient::MenuItemIDAutofillOptions);
  autofill_popup_view_->Show(autofill_values, autofill_values, autofill_values,
                             autofill_ids);

  // To remove warnings.
  EXPECT_CALL(*autofill_popup_view_, InvalidateRow(_)).Times(AtLeast(0));

  autofill_popup_view_->SetSelectedLine(0);

  // Make sure next skips the unselectable separator.
  autofill_popup_view_->SelectNextLine();
  EXPECT_EQ(2, autofill_popup_view_->selected_line());

  // Make sure previous skips the unselectable separator.
  autofill_popup_view_->SelectPreviousLine();
  EXPECT_EQ(0, autofill_popup_view_->selected_line());
}
