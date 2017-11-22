// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_view_native_views.h"

#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/views_test_base.h"

struct TypeClicks {
  autofill::PopupItemId id;
  int click;
};

const struct TypeClicks kClickTestCase[] = {
    {autofill::POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY, 1},
    {autofill::POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE, 1},
    {autofill::POPUP_ITEM_ID_PASSWORD_ENTRY, 1},
    {autofill::POPUP_ITEM_ID_SEPARATOR, 0},
    {autofill::POPUP_ITEM_ID_CLEAR_FORM, 1},
    {autofill::POPUP_ITEM_ID_AUTOFILL_OPTIONS, 1},
    {autofill::POPUP_ITEM_ID_DATALIST_ENTRY, 1},
    {autofill::POPUP_ITEM_ID_SCAN_CREDIT_CARD, 1},
    {autofill::POPUP_ITEM_ID_TITLE, 1},
    {autofill::POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO, 1},
    {autofill::POPUP_ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE, 1},
    {autofill::POPUP_ITEM_ID_USERNAME_ENTRY, 1},
    {autofill::POPUP_ITEM_ID_CREATE_HINT, 1},
    {autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY, 1},
    {autofill::POPUP_ITEM_ID_GENERATE_PASSWORD_ENTRY, 1},
};

class MockAutofillPopupController : public autofill::AutofillPopupController {
 public:
  MockAutofillPopupController() {
    gfx::FontList::SetDefaultFontDescription("Arial, Times New Roman, 15px");
    layout_model_ = std::make_unique<autofill::AutofillPopupLayoutModel>(
        this, false /* is_credit_card_field */);
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
    return suggestions_;
  }
#if !defined(OS_ANDROID)
  MOCK_METHOD1(GetElidedValueWidthForRow, int(int row));
  MOCK_METHOD1(GetElidedLabelWidthForRow, int(int row));
#endif

  // AutofillPopupController
  MOCK_METHOD0(OnSuggestionsChanged, void());
  MOCK_METHOD1(AcceptSuggestion, void(int index));

  int GetLineCount() const override { return suggestions_.size(); }

  const autofill::Suggestion& GetSuggestionAt(int row) const override {
    return suggestions_[row];
  }

  const base::string16& GetElidedValueAt(int i) const override {
    return suggestions_[i].value;
  }
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

  void set_suggestions(const std::vector<int>& ids) {
    for (const auto& id : ids)
      suggestions_.push_back(autofill::Suggestion("", "", "", id));
  }

 private:
  std::unique_ptr<autofill::AutofillPopupLayoutModel> layout_model_;
  std::vector<autofill::Suggestion> suggestions_;
};

class AutofillPopupViewNativeViewsTest : public views::ViewsTestBase {
 public:
  AutofillPopupViewNativeViewsTest() = default;
  ~AutofillPopupViewNativeViewsTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    CreateWidget();
    generator_.reset(new ui::test::EventGenerator(widget_.GetNativeWindow()));
  }

  void TearDown() override {
    generator_.reset();
    if (!widget_.IsClosed())
      widget_.Close();
    view_.reset();
    views::ViewsTestBase::TearDown();
  }

  void CreateAndShowView(const std::vector<int>& ids) {
    autofill_popup_controller_.set_suggestions(ids);
    view_ = base::MakeUnique<autofill::AutofillPopupViewNativeViews>(
        &autofill_popup_controller_, &widget_);
    widget_.SetContentsView(view_.get());

    widget_.Show();
  }

  autofill::AutofillPopupViewNativeViews* view() { return view_.get(); }

 protected:
  void CreateWidget() {
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_.Init(params);
  }

  std::unique_ptr<autofill::AutofillPopupViewNativeViews> view_;
  MockAutofillPopupController autofill_popup_controller_;
  views::Widget widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillPopupViewNativeViewsTest);
};

class AutofillPopupViewNativeViewsForEveryTypeTest
    : public AutofillPopupViewNativeViewsTest,
      public ::testing::WithParamInterface<TypeClicks> {};

TEST_F(AutofillPopupViewNativeViewsTest, ShowHideTest) {
  CreateAndShowView({0});
  EXPECT_CALL(autofill_popup_controller_, AcceptSuggestion(testing::_))
      .Times(0);
  view()->Hide();
}

TEST_P(AutofillPopupViewNativeViewsForEveryTypeTest, ShowClickTest) {
  const TypeClicks& click = GetParam();
  CreateAndShowView({click.id});
  EXPECT_CALL(autofill_popup_controller_, AcceptSuggestion(::testing::_))
      .Times(click.click);
  gfx::Point center = view()->child_at(0)->GetLocalBounds().CenterPoint();
  generator_->set_current_location(center);
  generator_->ClickLeftButton();
  view()->RemoveAllChildViews(true /* delete_children */);
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    AutofillPopupViewNativeViewsForEveryTypeTest,
    ::testing::ValuesIn(kClickTestCase));
