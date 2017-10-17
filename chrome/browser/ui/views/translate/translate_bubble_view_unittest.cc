// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_bubble_view.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/translate/translate_bubble_model.h"
#include "chrome/browser/ui/translate/translate_bubble_view_state_transition.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/range/range.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/combobox_test_api.h"
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
        always_translate_checked_(false),
        set_always_translate_called_count_(0),
        translate_called_(false),
        revert_translation_called_(false),
        translation_declined_(false),
        original_language_index_on_translation_(-1),
        target_language_index_on_translation_(-1),
        can_blacklist_site_(true) {}

  TranslateBubbleModel::ViewState GetViewState() const override {
    return view_state_transition_.view_state();
  }

  void SetViewState(TranslateBubbleModel::ViewState view_state) override {
    view_state_transition_.SetViewState(view_state);
  }

  void ShowError(translate::TranslateErrors::Type error_type) override {
    error_type_ = error_type;
  }

  void GoBackFromAdvanced() override {
    view_state_transition_.GoBackFromAdvanced();
  }

  int GetNumberOfLanguages() const override { return 1000; }

  base::string16 GetLanguageNameAt(int index) const override {
    return base::ASCIIToUTF16("English");
  }

  int GetOriginalLanguageIndex() const override {
    return original_language_index_;
  }

  void UpdateOriginalLanguageIndex(int index) override {
    original_language_index_ = index;
  }

  int GetTargetLanguageIndex() const override { return target_language_index_; }

  void UpdateTargetLanguageIndex(int index) override {
    target_language_index_ = index;
  }

  void DeclineTranslation() override { translation_declined_ = true; }

  void SetNeverTranslateLanguage(bool value) override {
    never_translate_language_ = value;
  }

  void SetNeverTranslateSite(bool value) override {
    never_translate_site_ = value;
  }

  bool ShouldAlwaysTranslateBeCheckedByDefault() const override {
    return always_translate_checked_;
  }

  bool ShouldAlwaysTranslate() const override {
    return should_always_translate_;
  }

  void SetAlwaysTranslate(bool value) override {
    should_always_translate_ = value;
    set_always_translate_called_count_++;
  }

  void Translate() override {
    translate_called_ = true;
    original_language_index_on_translation_ = original_language_index_;
    target_language_index_on_translation_ = target_language_index_;
  }

  void RevertTranslation() override { revert_translation_called_ = true; }

  void OnBubbleClosing() override {}

  bool IsPageTranslatedInCurrentLanguages() const override {
    return original_language_index_on_translation_ ==
               original_language_index_ &&
           target_language_index_on_translation_ == target_language_index_;
  }

  bool CanBlacklistSite() override { return can_blacklist_site_; }

  void SetCanBlacklistSite(bool value) { can_blacklist_site_ = value; }

  TranslateBubbleViewStateTransition view_state_transition_;
  translate::TranslateErrors::Type error_type_;
  int original_language_index_;
  int target_language_index_;
  bool never_translate_language_;
  bool never_translate_site_;
  bool should_always_translate_;
  bool always_translate_checked_;
  int set_always_translate_called_count_;
  bool translate_called_;
  bool revert_translation_called_;
  bool translation_declined_;
  int original_language_index_on_translation_;
  int target_language_index_on_translation_;
  bool can_blacklist_site_;
};

}  // namespace

class TranslateBubbleViewTest : public views::ViewsTestBase {
 public:
  TranslateBubbleViewTest() {}

 protected:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    // Set the ChromeLayoutProvider as the default layout provider.
    test_views_delegate()->set_layout_provider(
        ChromeLayoutProvider::CreateLayoutProvider());

    // The bubble needs the parent as an anchor.
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

    anchor_widget_.reset(new views::Widget());
    anchor_widget_->Init(params);
    anchor_widget_->Show();

    mock_model_ = new MockTranslateBubbleModel(
        TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE);
  }

  void TurnOnTranslate2016Q2UIFlag() {
    scoped_feature_list_.InitAndEnableFeature(translate::kTranslateUI2016Q2);
  }

  void CreateAndShowBubble() {
    std::unique_ptr<TranslateBubbleModel> model(mock_model_);
    bubble_ = new TranslateBubbleView(anchor_widget_->GetContentsView(),
                                      gfx::Point(), std::move(model),
                                      translate::TranslateErrors::NONE, NULL);
    views::BubbleDialogDelegateView::CreateBubble(bubble_)->Show();
  }

  void TearDown() override {
    bubble_->GetWidget()->CloseNow();
    anchor_widget_.reset();

    views::ViewsTestBase::TearDown();
  }

  views::Combobox* denial_combobox() { return bubble_->denial_combobox_; }
  bool denial_button_clicked() { return mock_model_->translation_declined_; }

  std::unique_ptr<views::Widget> anchor_widget_;
  MockTranslateBubbleModel* mock_model_;
  TranslateBubbleView* bubble_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TranslateBubbleViewTest, TranslateButton) {
  CreateAndShowBubble();
  EXPECT_FALSE(mock_model_->translate_called_);

  // Press the "Translate" button.
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_TRANSLATE);
  EXPECT_TRUE(mock_model_->translate_called_);
}

TEST_F(TranslateBubbleViewTest, TranslateButtonIn2016Q2UI) {
  TurnOnTranslate2016Q2UIFlag();
  CreateAndShowBubble();
  EXPECT_FALSE(mock_model_->translate_called_);

  // Press the "Translate" button.
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_TRANSLATE);
  EXPECT_TRUE(mock_model_->translate_called_);
}

TEST_F(TranslateBubbleViewTest, CloseButtonIn2016Q2UI) {
  TurnOnTranslate2016Q2UIFlag();
  CreateAndShowBubble();
  EXPECT_FALSE(mock_model_->translate_called_);
  EXPECT_FALSE(mock_model_->translation_declined_);
  EXPECT_FALSE(bubble_->GetWidget()->IsClosed());

  // Press the "Close" button.
  bubble_->GetBubbleFrameView()->ButtonPressed(
      bubble_->GetBubbleFrameView()->GetCloseButtonForTest(),
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE));

  EXPECT_FALSE(mock_model_->translate_called_);
  EXPECT_TRUE(mock_model_->translation_declined_);
}

TEST_F(TranslateBubbleViewTest, ComboboxNope) {
  CreateAndShowBubble();
  views::test::ComboboxTestApi test_api(denial_combobox());
  EXPECT_FALSE(denial_button_clicked());
  EXPECT_FALSE(bubble_->GetWidget()->IsClosed());

  test_api.PerformActionAt(static_cast<int>(
      TranslateBubbleView::DenialComboboxIndex::DONT_TRANSLATE));
  EXPECT_TRUE(denial_button_clicked());
  EXPECT_TRUE(bubble_->GetWidget()->IsClosed());
}

TEST_F(TranslateBubbleViewTest, ComboboxNeverTranslateLanguage) {
  CreateAndShowBubble();
  views::test::ComboboxTestApi test_api(denial_combobox());
  EXPECT_FALSE(bubble_->GetWidget()->IsClosed());
  EXPECT_FALSE(mock_model_->never_translate_language_);
  EXPECT_FALSE(denial_button_clicked());

  test_api.PerformActionAt(static_cast<int>(
      TranslateBubbleView::DenialComboboxIndex::NEVER_TRANSLATE_LANGUAGE));
  EXPECT_TRUE(denial_button_clicked());
  EXPECT_TRUE(mock_model_->never_translate_language_);
  EXPECT_TRUE(bubble_->GetWidget()->IsClosed());
}

TEST_F(TranslateBubbleViewTest, ComboboxNeverTranslateSite) {
  CreateAndShowBubble();
  views::test::ComboboxTestApi test_api(denial_combobox());
  EXPECT_FALSE(mock_model_->never_translate_site_);
  EXPECT_FALSE(denial_button_clicked());
  EXPECT_FALSE(bubble_->GetWidget()->IsClosed());

  test_api.PerformActionAt(static_cast<int>(
      TranslateBubbleView::DenialComboboxIndex::NEVER_TRANSLATE_SITE));
  EXPECT_TRUE(denial_button_clicked());
  EXPECT_TRUE(mock_model_->never_translate_site_);
  EXPECT_TRUE(bubble_->GetWidget()->IsClosed());
}

TEST_F(TranslateBubbleViewTest, MenuButtonNeverTranslateLanguage) {
  TurnOnTranslate2016Q2UIFlag();
  CreateAndShowBubble();
  EXPECT_FALSE(bubble_->GetWidget()->IsClosed());
  EXPECT_FALSE(mock_model_->never_translate_language_);
  EXPECT_FALSE(denial_button_clicked());

  bubble_->ExecuteCommand(
      TranslateBubbleView::DenialMenuItem::NEVER_TRANSLATE_LANGUAGE, 0);

  EXPECT_TRUE(denial_button_clicked());
  EXPECT_TRUE(mock_model_->never_translate_language_);
  EXPECT_TRUE(bubble_->GetWidget()->IsClosed());
}

TEST_F(TranslateBubbleViewTest, MenuButtonNeverTranslateSite) {
  TurnOnTranslate2016Q2UIFlag();
  CreateAndShowBubble();
  EXPECT_FALSE(mock_model_->never_translate_site_);
  EXPECT_FALSE(denial_button_clicked());
  EXPECT_FALSE(bubble_->GetWidget()->IsClosed());

  bubble_->ExecuteCommand(
      TranslateBubbleView::DenialMenuItem::NEVER_TRANSLATE_SITE, 0);

  EXPECT_TRUE(denial_button_clicked());
  EXPECT_TRUE(mock_model_->never_translate_site_);
  EXPECT_TRUE(bubble_->GetWidget()->IsClosed());
}

TEST_F(TranslateBubbleViewTest, AdvancedLink) {
  CreateAndShowBubble();
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());

  // Click the "Advanced" link.
  bubble_->HandleLinkClicked(TranslateBubbleView::LINK_ID_ADVANCED);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_ADVANCED, bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, AdvancedLinkIn2016Q2UI) {
  TurnOnTranslate2016Q2UIFlag();
  CreateAndShowBubble();
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());

  // Click the styled label link.
  views::StyledLabel styled_label(base::ASCIIToUTF16("test"), nullptr);
  bubble_->StyledLabelLinkClicked(&styled_label, gfx::Range(), ui::EF_NONE);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_ADVANCED, bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, ShowOriginalButton) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);

  // Click the "Show original" button to revert translation.
  EXPECT_FALSE(mock_model_->revert_translation_called_);
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_SHOW_ORIGINAL);
  EXPECT_TRUE(mock_model_->revert_translation_called_);
}

TEST_F(TranslateBubbleViewTest, TryAgainButton) {
  CreateAndShowBubble();
  bubble_->SwitchToErrorView(translate::TranslateErrors::NETWORK);

  EXPECT_EQ(translate::TranslateErrors::NETWORK, mock_model_->error_type_);

  // Click the "Try again" button to translate.
  EXPECT_FALSE(mock_model_->translate_called_);
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_TRY_AGAIN);
  EXPECT_TRUE(mock_model_->translate_called_);
}

TEST_F(TranslateBubbleViewTest, AlwaysTranslateCheckboxAndCancelButton) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Always Translate" checkbox. Changing the state of this checkbox
  // should NOT affect the model after pressing the cancel button.

  // Check the initial state.
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);
  EXPECT_FALSE(bubble_->advanced_always_translate_checkbox_->checked());

  // Click the checkbox. The state is not saved yet.
  bubble_->advanced_always_translate_checkbox_->SetChecked(true);
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_ALWAYS_TRANSLATE);
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);

  // Click the cancel button. The state is not saved.
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_CANCEL);
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);
}

TEST_F(TranslateBubbleViewTest, AlwaysTranslateCheckboxAndDoneButton) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Always Translate" checkbox. Changing the state of this checkbox
  // should affect the model after pressing the done button.

  // Check the initial state.
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);
  EXPECT_FALSE(bubble_->advanced_always_translate_checkbox_->checked());

  // Click the checkbox. The state is not saved yet.
  bubble_->advanced_always_translate_checkbox_->SetChecked(true);
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_ALWAYS_TRANSLATE);
  EXPECT_FALSE(mock_model_->should_always_translate_);
  EXPECT_EQ(0, mock_model_->set_always_translate_called_count_);

  // Click the done button. The state is saved.
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_DONE);
  EXPECT_TRUE(mock_model_->should_always_translate_);
  EXPECT_EQ(1, mock_model_->set_always_translate_called_count_);
}

TEST_F(TranslateBubbleViewTest, DoneButton) {
  CreateAndShowBubble();
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
  CreateAndShowBubble();
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
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE);
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Cancel" button to go back.
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_ADVANCED, bubble_->GetViewState());
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_CANCEL);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_BEFORE_TRANSLATE,
            bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, CancelButtonReturningAfterTranslate) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE);
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Cancel" button to go back.
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_ADVANCED, bubble_->GetViewState());
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_CANCEL);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE,
            bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, CancelButtonReturningError) {
  CreateAndShowBubble();
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ERROR);
  bubble_->SwitchView(TranslateBubbleModel::VIEW_STATE_ADVANCED);

  // Click the "Cancel" button to go back.
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_ADVANCED, bubble_->GetViewState());
  bubble_->HandleButtonPressed(TranslateBubbleView::BUTTON_ID_CANCEL);
  EXPECT_EQ(TranslateBubbleModel::VIEW_STATE_ERROR, bubble_->GetViewState());
}

TEST_F(TranslateBubbleViewTest, ComboboxCanBlacklistSite) {
  EXPECT_TRUE(mock_model_->CanBlacklistSite());
  CreateAndShowBubble();
  views::Combobox* const combobox = denial_combobox();
  EXPECT_FALSE(denial_button_clicked());
  EXPECT_FALSE(bubble_->GetWidget()->IsClosed());
  // Check menu rows are DENY, NEVER_TRANSLATE_LANG, [SEPARATOR], and
  // NEVER_TRANSLATE_SITE.
  EXPECT_EQ(4, combobox->GetRowCount());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_DENY),
            combobox->GetTextForRow(static_cast<size_t>(
                TranslateBubbleView::DenialComboboxIndex::DONT_TRANSLATE)));
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_LANG,
                                 base::ASCIIToUTF16("English")),
      combobox->GetTextForRow(static_cast<size_t>(
          TranslateBubbleView::DenialComboboxIndex::NEVER_TRANSLATE_LANGUAGE)));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_SITE),
      combobox->GetTextForRow(static_cast<size_t>(
          TranslateBubbleView::DenialComboboxIndex::NEVER_TRANSLATE_SITE)));
}

TEST_F(TranslateBubbleViewTest, ComboboxCantBlacklistSite) {
  mock_model_->SetCanBlacklistSite(false);
  CreateAndShowBubble();
  views::Combobox* const combobox = denial_combobox();
  //  views::test::ComboboxTestApi test_api(combobox);
  EXPECT_FALSE(denial_button_clicked());
  EXPECT_FALSE(bubble_->GetWidget()->IsClosed());
  // Check the menu rows are DENY and NEVER_TRANSLATE_LANG.
  EXPECT_EQ(2, combobox->GetRowCount());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_TRANSLATE_BUBBLE_DENY),
            combobox->GetTextForRow(static_cast<size_t>(
                TranslateBubbleView::DenialComboboxIndex::DONT_TRANSLATE)));
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_TRANSLATE_BUBBLE_NEVER_TRANSLATE_LANG,
                                 base::ASCIIToUTF16("English")),
      combobox->GetTextForRow(static_cast<size_t>(
          TranslateBubbleView::DenialComboboxIndex::NEVER_TRANSLATE_LANGUAGE)));
}
