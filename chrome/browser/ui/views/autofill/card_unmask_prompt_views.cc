// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_models.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_controller.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_view.h"
#include "chrome/browser/ui/views/autofill/decorated_textfield.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace autofill {

namespace {

class CardUnmaskPromptViews : public CardUnmaskPromptView,
                              views::ComboboxListener,
                              views::DialogDelegateView,
                              views::TextfieldController {
 public:
  explicit CardUnmaskPromptViews(CardUnmaskPromptController* controller)
      : controller_(controller),
        cvc_input_(nullptr),
        month_input_(nullptr),
        year_input_(nullptr),
        message_label_(nullptr),
        storage_checkbox_(nullptr) {}

  ~CardUnmaskPromptViews() override {
    if (controller_)
      controller_->OnUnmaskDialogClosed();
  }

  void Show() {
    constrained_window::ShowWebModalDialogViews(this,
                                                controller_->GetWebContents());
  }

  // CardUnmaskPromptView
  void ControllerGone() override {
    controller_ = nullptr;
    ClosePrompt();
  }

  void DisableAndWaitForVerification() override {
    SetInputsEnabled(false);
    message_label_->SetText(base::ASCIIToUTF16("Verifying..."));
    message_label_->SetVisible(true);
    GetDialogClientView()->UpdateDialogButtons();
    Layout();
  }

  void GotVerificationResult(bool success) override {
    if (success) {
      message_label_->SetText(base::ASCIIToUTF16("Success!"));
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE, base::Bind(&CardUnmaskPromptViews::ClosePrompt,
                                base::Unretained(this)),
          base::TimeDelta::FromSeconds(1));
    } else {
      SetInputsEnabled(true);
      cvc_input_->SetInvalid(true);
      message_label_->SetText(base::ASCIIToUTF16("Verification error."));
      GetDialogClientView()->UpdateDialogButtons();
    }
    Layout();
  }

  void SetInputsEnabled(bool enabled) {
    cvc_input_->SetEnabled(enabled);

    if (month_input_)
      month_input_->SetEnabled(enabled);
    if (year_input_)
      year_input_->SetEnabled(enabled);
  }

  // views::DialogDelegateView
  View* GetContentsView() override {
    InitIfNecessary();
    return this;
  }

  // views::View
  gfx::Size GetPreferredSize() const override {
    // Must hardcode a width so the label knows where to wrap. TODO(estade):
    // This can lead to a weird looking dialog if we end up getting allocated
    // more width than we ask for, e.g. if the title is super long.
    const int kWidth = 250;
    return gfx::Size(kWidth, GetHeightForWidth(kWidth));
  }

  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_CHILD; }

  base::string16 GetWindowTitle() const override {
    return controller_->GetWindowTitle();
  }

  void DeleteDelegate() override { delete this; }

  int GetDialogButtons() const override {
    return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
  }

  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override {
    // TODO(estade): fix this.
    if (button == ui::DIALOG_BUTTON_OK)
      return base::ASCIIToUTF16("Verify");

    return DialogDelegateView::GetDialogButtonLabel(button);
  }

  bool ShouldDefaultButtonBeBlue() const override { return true; }

  bool IsDialogButtonEnabled(ui::DialogButton button) const override {
    if (button == ui::DIALOG_BUTTON_CANCEL)
      return true;

    DCHECK_EQ(ui::DIALOG_BUTTON_OK, button);

    return cvc_input_->enabled() &&
           controller_->InputTextIsValid(cvc_input_->text()) &&
           (!month_input_ ||
            month_input_->selected_index() !=
                month_combobox_model_.GetDefaultIndex()) &&
           (!year_input_ ||
            year_input_->selected_index() !=
                year_combobox_model_.GetDefaultIndex());
  }

  views::View* GetInitiallyFocusedView() override { return cvc_input_; }

  bool Cancel() override {
    return true;
  }

  bool Accept() override {
    if (!controller_)
      return true;

    controller_->OnUnmaskResponse(
        cvc_input_->text(),
        month_input_
            ? month_combobox_model_.GetItemAt(month_input_->selected_index())
            : base::string16(),
        year_input_
            ? year_combobox_model_.GetItemAt(year_input_->selected_index())
            : base::string16(),
        storage_checkbox_ ? storage_checkbox_->checked() : false);
    return false;
  }

  // views::TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override {
    GetDialogClientView()->UpdateDialogButtons();
  }

  // views::ComboboxListener
  void OnPerformAction(views::Combobox* combobox) override {
    GetDialogClientView()->UpdateDialogButtons();
  }

 private:
  void InitIfNecessary() {
    if (has_children())
      return;

    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 19, 0, 5));
    views::Label* instructions =
        new views::Label(controller_->GetInstructionsMessage());

    instructions->SetMultiLine(true);
    instructions->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(instructions);

    views::View* input_row = new views::View();
    input_row->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 5));
    AddChildView(input_row);

    if (controller_->ShouldRequestExpirationDate()) {
      month_input_ = new views::Combobox(&month_combobox_model_);
      month_input_->set_listener(this);
      input_row->AddChildView(month_input_);
      year_input_ = new views::Combobox(&year_combobox_model_);
      year_input_->set_listener(this);
      input_row->AddChildView(year_input_);
    }

    cvc_input_ = new DecoratedTextfield(
        base::string16(),
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC),
        this);
    cvc_input_->set_default_width_in_chars(10);
    input_row->AddChildView(cvc_input_);

    views::ImageView* cvc_image = new views::ImageView();
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

    cvc_image->SetImage(rb.GetImageSkiaNamed(controller_->GetCvcImageRid()));

    input_row->AddChildView(cvc_image);

    message_label_ = new views::Label();
    input_row->AddChildView(message_label_);
    message_label_->SetVisible(false);

    storage_checkbox_ = new views::Checkbox(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_CARD_UNMASK_PROMPT_STORAGE_CHECKBOX));
    storage_checkbox_->SetChecked(controller_->GetStoreLocallyStartState());
    AddChildView(storage_checkbox_);
  }

  void ClosePrompt() { GetWidget()->Close(); }

  CardUnmaskPromptController* controller_;

  DecoratedTextfield* cvc_input_;

  // These will be null when expiration date is not required.
  views::Combobox* month_input_;
  views::Combobox* year_input_;

  MonthComboboxModel month_combobox_model_;
  YearComboboxModel year_combobox_model_;

  // TODO(estade): this is a temporary standin in place of some spinner UI
  // as well as a better error message.
  views::Label* message_label_;

  views::Checkbox* storage_checkbox_;

  DISALLOW_COPY_AND_ASSIGN(CardUnmaskPromptViews);
};

}  // namespace

// static
CardUnmaskPromptView* CardUnmaskPromptView::CreateAndShow(
    CardUnmaskPromptController* controller) {
  CardUnmaskPromptViews* view = new CardUnmaskPromptViews(controller);
  view->Show();
  return view;
}

}  // namespace autofill
