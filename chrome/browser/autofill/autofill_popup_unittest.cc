// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_popup_view.h"
#include "chrome/browser/autofill/test_autofill_external_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class MockAutofillExternalDelegate : public TestAutofillExternalDelegate {
 public:
  MockAutofillExternalDelegate() : TestAutofillExternalDelegate(NULL, NULL) {};
  virtual ~MockAutofillExternalDelegate() {};

  virtual void SelectAutofillSuggestionAtIndex(int unique_id, int list_index)
      OVERRIDE {}
};

class TestAutofillPopupView : public AutofillPopupView {
 public:
  explicit TestAutofillPopupView(AutofillExternalDelegate* external_delegate) :
      AutofillPopupView(NULL, external_delegate) {
    std::vector<string16> autofill_values;
    autofill_values.push_back(string16());
    autofill_values.push_back(string16());

    std::vector<int> autofill_ids;
    autofill_ids.push_back(0);
    autofill_ids.push_back(1);

    Show(autofill_values, autofill_values, autofill_values, autofill_ids, 0);
  }
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

  MOCK_METHOD1(InvalidateRow, void(size_t));

 private:
  virtual void ShowInternal() OVERRIDE {}
  virtual void HideInternal() OVERRIDE {}
};

}  // namespace

class AutofillPopupViewUnitTest : public ::testing::Test {
 public:
  AutofillPopupViewUnitTest() {
    autofill_popup_view_.reset(new TestAutofillPopupView(&external_delegate_));
  }
  virtual ~AutofillPopupViewUnitTest() {}

  scoped_ptr<TestAutofillPopupView> autofill_popup_view_;

 private:
  MockAutofillExternalDelegate external_delegate_;
};

TEST_F(AutofillPopupViewUnitTest, ChangeSelectedLine) {
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
