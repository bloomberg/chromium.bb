// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_CARD_UNMASK_PROMPT_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_CARD_UNMASK_PROMPT_VIEWS_H_

#include "chrome/browser/ui/autofill/autofill_dialog_models.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_view.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
class Checkbox;
}

namespace autofill {

class DecoratedTextfield;

class CardUnmaskPromptViews : public CardUnmaskPromptView,
                              views::ComboboxListener,
                              views::DialogDelegateView,
                              views::TextfieldController {
 public:
  explicit CardUnmaskPromptViews(CardUnmaskPromptController* controller);

  ~CardUnmaskPromptViews() override;

  void Show();

  // CardUnmaskPromptView
  void ControllerGone() override;
  void DisableAndWaitForVerification() override;
  void GotVerificationResult(const base::string16& error_message,
                             bool allow_retry) override;

  // views::DialogDelegateView
  View* GetContentsView() override;
  views::View* CreateFootnoteView() override;

  // views::View
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  int GetHeightForWidth(int width) const override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  void DeleteDelegate() override;
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool ShouldDefaultButtonBeBlue() const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  views::View* GetInitiallyFocusedView() override;
  bool Cancel() override;
  bool Accept() override;

  // views::TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  // views::ComboboxListener
  void OnPerformAction(views::Combobox* combobox) override;

 private:
  friend class CardUnmaskPromptViewTesterViews;

  void InitIfNecessary();
  void SetRetriableErrorMessage(const base::string16& message);
  bool ExpirationDateIsValid() const;
  void SetInputsEnabled(bool enabled);
  void ClosePrompt();

  CardUnmaskPromptController* controller_;

  views::View* main_contents_;

  // The error label for permanent errors (where the user can't retry).
  views::Label* permanent_error_label_;

  DecoratedTextfield* cvc_input_;

  // These will be null when expiration date is not required.
  views::Combobox* month_input_;
  views::Combobox* year_input_;

  MonthComboboxModel month_combobox_model_;
  YearComboboxModel year_combobox_model_;

  // The error label for most errors, which lives beneath the inputs.
  views::Label* error_label_;

  views::Checkbox* storage_checkbox_;

  views::View* progress_overlay_;
  views::Label* progress_label_;

  base::WeakPtrFactory<CardUnmaskPromptViews> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CardUnmaskPromptViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_CARD_UNMASK_PROMPT_VIEWS_H_
