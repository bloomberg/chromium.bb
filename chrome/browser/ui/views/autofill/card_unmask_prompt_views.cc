// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_controller.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace autofill {

namespace {

class CardUnmaskPromptViews : public CardUnmaskPromptView,
                              views::DialogDelegateView,
                              views::TextfieldController {
 public:
  explicit CardUnmaskPromptViews(CardUnmaskPromptController* controller)
      : controller_(controller), cvc_(nullptr), message_label_(nullptr) {}

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
    GetWidget()->Close();
  }

  void DisableAndWaitForVerification() override {
    cvc_->SetEnabled(false);
    message_label_->SetText(base::ASCIIToUTF16("Verifying..."));
    message_label_->SetVisible(true);
    GetDialogClientView()->UpdateDialogButtons();
    Layout();
  }

  void VerificationSucceeded() override {
    // TODO(estade): implement.
  }

  void VerificationFailed() override {
    cvc_->SetEnabled(true);
    message_label_->SetText(
        base::ASCIIToUTF16("Verification error. Please try again."));
    message_label_->SetVisible(true);
    GetDialogClientView()->UpdateDialogButtons();
    Layout();
  }

  // views::DialogDelegateView
  View* GetContentsView() override {
    InitIfNecessary();
    return this;
  }

  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_CHILD; }

  base::string16 GetWindowTitle() const override {
    // TODO(estade): fix this.
    return base::ASCIIToUTF16("Unlocking Visa - 1111");
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

    base::string16 input_text;
    base::TrimWhitespace(cvc_->text(), base::TRIM_ALL, &input_text);
    return cvc_->enabled() && input_text.size() == 3 &&
           base::ContainsOnlyChars(input_text,
                                   base::ASCIIToUTF16("0123456789"));
  }

  views::View* GetInitiallyFocusedView() override { return cvc_; }

  bool Cancel() override {
    return true;
  }

  bool Accept() override {
    if (!controller_)
      return true;

    controller_->OnUnmaskResponse(cvc_->text());
    return false;
  }

  // views::TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override {
    GetDialogClientView()->UpdateDialogButtons();
  }

 private:
  void InitIfNecessary() {
    if (has_children())
      return;

    // TODO(estade): for amex, text and CVC image should be different.
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 19, 0, 5));
    AddChildView(new views::Label(base::ASCIIToUTF16(
        "Enter the three digit verification code from the back of your credit "
        "card")));

    views::View* cvc_container = new views::View();
    cvc_container->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 5));
    AddChildView(cvc_container);

    cvc_ = new views::Textfield();
    cvc_->set_controller(this);
    cvc_->set_placeholder_text(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC));
    cvc_->set_default_width_in_chars(10);
    cvc_container->AddChildView(cvc_);

    views::ImageView* cvc_image = new views::ImageView();
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    cvc_image->SetImage(rb.GetImageSkiaNamed(IDR_CREDIT_CARD_CVC_HINT));
    cvc_container->AddChildView(cvc_image);

    message_label_ = new views::Label();
    cvc_container->AddChildView(message_label_);
    message_label_->SetVisible(false);
  }

  CardUnmaskPromptController* controller_;

  views::Textfield* cvc_;

  // TODO(estade): this is a temporary standin in place of some spinner UI
  // as well as a better error message.
  views::Label* message_label_;

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
