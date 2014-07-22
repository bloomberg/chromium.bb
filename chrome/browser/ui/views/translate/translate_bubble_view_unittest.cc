// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_bubble_view.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/translate/translate_bubble_view_state_transition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace {

class MockTranslateBubbleModel : public TranslateBubbleModel {
 public:
  explicit MockTranslateBubbleModel(TranslateBubbleModel::ViewState view_state)
      : view_state_transition_(view_state),
        error_type_(translate::TranslateErrors::NONE),
        original_language_index_(0),
        target_language_index_(1),
        never_translate_language_(false),
        never_translate_site_(false),
        should_always_translate_(false),
        set_always_translate_called_count_(0),
        translate_called_(false),
        revert_translation_called_(false),
        translation_declined_called_(false),
        original_language_index_on_translation_(-1),
        target_language_index_on_translation_(-1) {}

  virtual TranslateBubbleModel::ViewState GetViewState() const OVERRIDE {
    return view_state_transition_.view_state();
  }

  virtual void SetViewState(TranslateBubbleModel::ViewState view_state)
      OVERRIDE {
    view_state_transition_.SetViewState(view_state);
  }

  virtual void ShowError(translate::TranslateErrors::Type error_type) OVERRIDE {
    error_type_ = error_type;
  }

  virtual void GoBackFromAdvanced() OVERRIDE {
    view_state_transition_.GoBackFromAdvanced();
  }

  virtual int GetNumberOfLanguages() const OVERRIDE {
    return 1000;
  }

  virtual base::string16 GetLanguageNameAt(int index) const OVERRIDE {
    return base::string16();
  }

  virtual int GetOriginalLanguageIndex() const OVERRIDE {
    return original_language_index_;
  }

  virtual void UpdateOriginalLanguageIndex(int index) OVERRIDE {
    original_language_index_ = index;
  }

  virtual int GetTargetLanguageIndex() const OVERRIDE {
    return target_language_index_;
  }

  virtual void UpdateTargetLanguageIndex(int index) OVERRIDE {
    target_language_index_ = index;
  }

  virtual void SetNeverTranslateLanguage(bool value) OVERRIDE {
    never_translate_language_ = value;
  }

  virtual void SetNeverTranslateSite(bool value) OVERRIDE {
    never_translate_site_ = value;
  }

  virtual bool ShouldAlwaysTranslate() const OVERRIDE {
    return should_always_translate_;
  }

  virtual void SetAlwaysTranslate(bool value) OVERRIDE {
    should_always_translate_ = value;
    set_always_translate_called_count_++;
  }

  virtual void Translate() OVERRIDE {
    translate_called_ = true;
    original_language_index_on_translation_ = original_language_index_;
    target_language_index_on_translation_ = target_language_index_;
  }

  virtual void RevertTranslation() OVERRIDE {
    revert_translation_called_ = true;
  }

  virtual void TranslationDeclined(bool explicitly_closed) OVERRIDE {
    translation_declined_called_ = true;
  }

  virtual bool IsPageTranslatedInCurrentLanguages() const OVERRIDE {
    return original_language_index_on_translation_ ==
        original_language_index_ &&
        target_language_index_on_translation_ == target_language_index_;
  }

  TranslateBubbleViewStateTransition view_state_transition_;
  translate::TranslateErrors::Type error_type_;
  int original_language_index_;
  int target_language_index_;
  bool never_translate_language_;
  bool never_translate_site_;
  bool should_always_translate_;
  int set_always_translate_called_count_;
  bool translate_called_;
  bool revert_translation_called_;
  bool translation_declined_called_;
  int original_language_index_on_translation_;
  int target_language_index_on_translation_;
};

}  // namespace

class TranslateBubbleViewTest : public views::ViewsTestBase {
 public:
  TranslateBubbleViewTest() {
  }

 protected:
  virtual void SetUp() OVERRIDE {
    views::ViewsTestBase::SetUp();

    // The bubble needs the parent as an anchor.
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

    anchor_widget_.reset(new views::Widget());
    anchor_widget_->Init(params);
    anchor_widget_->Show();

    mock_model_ = new MockTranslateBubbleModel(
        TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE);
    scoped_ptr<TranslateBubbleModel> model(mock_model_);
    bubble_ = new TranslateBubbleView(anchor_widget_->GetContentsView(),
                                      model.Pass(),
                                      translate::TranslateErrors::NONE,
                                      NULL);
    views::BubbleDelegateView::CreateBubble(bubble_)->Show();
  }

  virtual void TearDown() OVERRIDE {
    bubble_->GetWidget()->CloseNow();
    anchor_widget_.reset();

    views::ViewsTestBase::TearDown();
  }

  scoped_ptr<views::Widget> anchor_widget_;
  MockTranslateBubbleModel* mock_model_;
  TranslateBubbleView* bubble_;
};

TEST_F(TranslateBubbleViewTest, TranslateButton) {
  EXPECT_FALSE(mock_model_->translate_called_);

  // Press the "Translate" button.
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_TRANSLATE);
  EXPECT_TRUE(mock_model_->translate_called_);
}

TEST_F(TranslateBubbleViewTest, AdvancedLink) {
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());

  // Click the "Advanced" link.
  bubble_->HandleLinkClicked(TranslateBubbleView::LINK_ID_ADVANCED);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_ADVANCED, bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, ShowOriginalButton) {
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);

  // Click the "Show original" button to revert translation.
  EXPECT_FALSE(mock_model_->revert_translation_called_);
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_SHOW_ORIGINAL);
  EXPECT_TRUE(mock_model_->revert_translation_called_);
}

TEST_F(TranslateBubbleViewTest, TryAgainButton) {
  bubble_->SwitchToErrorView(translate::TranslateErrors::NETWORK);

  EXPECT_EQ(translate::TranslateErrors::NETWORK, mock_model_->error_type_);

  // Click the "Try again" button to translate.
  EXPECT_FALSE(mock_model_->translate_called_);
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_TRY_AGAIN);
  EXPECT_TRUE(mock_model_->translate_called_);
}

TEST_F(TranslateBubbleViewTest, AlwaysTranslateCheckboxAndCancelButton) {
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Always Translate" checkbox. Changing the state of this checkbox
  // should NOT affect the model after pressing the cancel button.

  // Check the initial state.
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);
  EXPECT_FALSE(bubble_->always_translate_checkbox_->checked());

  // Click the checkbox. The state is not saved yet.
  bubble_->always_translate_checkbox_->SetChecked(true);
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_ALWAYS_TRANSLATE);
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);

  // Click the cancel button. The state is not saved.
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_CANCEL);
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);
}

TEST_F(TranslateBubbleViewTest, AlwaysTranslateCheckboxAndDoneButton) {
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Always Translate" checkbox. Changing the state of this checkbox
  // should affect the model after pressing the done button.

  // Check the initial state.
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);
  EXPECT_FALSE(bubble_->always_translate_checkbox_->checked());

  // Click the checkbox. The state is not saved yet.
  bubble_->always_translate_checkbox_->SetChecked(true);
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_ALWAYS_TRANSLATE);
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);

  // Click the done button. The state is saved.
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_DONE);
  EXPECT_TRUE(mock_model_->should_always_translate_);
  EXPECT_EQ(1, mock_model_->set_always_translate_called_count_);
}

TEST_F(TranslateBubbleViewTest, DoneButton) {
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Done" button to translate. The selected languages by the user
  // are applied.
  EXPECT_FALSE(mock_model_->translate_called_);
  bubble_->source_language_combobox_->SetSelectedIndex(10);
  bubble_->HandleComboboxPerformAction(
      TranslateBubbleView::COMBOBOX_ID_SOURCE_LANGUAGE);
  bubble_->target_language_combobox_->SetSelectedIndex(20);
  bubble_->HandleComboboxPerformAction(
      TranslateBubbleView::COMBOBOX_ID_TARGET_LANGUAGE);
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_DONE);
  EXPECT_TRUE(mock_model_->translate_called_);
  EXPECT_EQ(10, mock_model_->original_language_index_);
  EXPECT_EQ(20, mock_model_->target_language_index_);
}

TEST_F(TranslateBubbleViewTest, DoneButtonWithoutTranslating) {
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());

  // Translate the page once.
  mock_model_->Translate();
  EXPECT_TRUE(mock_model_->translate_called_);

  // Go back to the initial view.
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());
  mock_model_->translate_called_ = false;

  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Done" button with the current language pair. This time,
  // translation is not performed and the view state will be back to the
  // previous view.
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_DONE);
  EXPECT_FALSE(mock_model_->translate_called_);

  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, CancelButtonReturningBeforeTranslate) {
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE);
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Cancel" button to go back.
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_ADVANCED, bubble_->GetViewState());
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_CANCEL);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, CancelButtonReturningAfterTranslate) {
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Cancel" button to go back.
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_ADVANCED, bubble_->GetViewState());
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_CANCEL);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE,
            bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, CancelButtonReturningError) {
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ERROR);
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Cancel" button to go back.
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_ADVANCED, bubble_->GetViewState());
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_CANCEL);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_ERROR, bubble_->GetViewState());
}
