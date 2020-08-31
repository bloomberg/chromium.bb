// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/virtual_card_selection_dialog_controller_impl.h"

#include "chrome/browser/ui/autofill/payments/virtual_card_selection_dialog_view.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

VirtualCardSelectionDialogControllerImpl::
    VirtualCardSelectionDialogControllerImpl(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

VirtualCardSelectionDialogControllerImpl::
    ~VirtualCardSelectionDialogControllerImpl() {
  // This part of code is executed only if browser window is closed when the
  // dialog is visible. In this case the controller is destroyed before
  // VirtualCardSelectionDialogViewImpl::dtor() being called, but the reference
  // to controller is not reset. Need to reset via
  // VirtualCardSelectionDialogViewImpl::Hide() to avoid crash.
  if (dialog_view_)
    dialog_view_->Hide();
}

void VirtualCardSelectionDialogControllerImpl::ShowDialog(
    const std::vector<CreditCard*>& candidates,
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK(!dialog_view_);

  candidates_ = candidates;
  // If there is only one card available, the card will be selected by default.
  // TODO(crbug.com/1020740): Change this to |instrument_token| when a card can
  // have multiple cloud token data.
  if (candidates_.size() == 1)
    selected_card_id_ = candidates_[0]->server_id();

  callback_ = std::move(callback);
  dialog_view_ =
      VirtualCardSelectionDialogView::CreateAndShow(this, web_contents());
}

bool VirtualCardSelectionDialogControllerImpl::IsOkButtonEnabled() {
  // The OK button is set to be disabled at first when there are multiple
  // cards. A card must be selected to enable the OK button.
  return !selected_card_id_.empty();
}

base::string16 VirtualCardSelectionDialogControllerImpl::GetContentTitle()
    const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_SELECTION_DIALOG_CONTENT_TITLE,
      candidates_.size());
}

base::string16 VirtualCardSelectionDialogControllerImpl::GetContentExplanation()
    const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_SELECTION_DIALOG_CONTENT_EXPLANATION,
      candidates_.size());
}

base::string16 VirtualCardSelectionDialogControllerImpl::GetOkButtonLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_SELECTION_DIALOG_OK_BUTTON_LABEL);
}

base::string16 VirtualCardSelectionDialogControllerImpl::GetCancelButtonLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_SELECTION_DIALOG_CANCEL_BUTTON_LABEL);
}

const std::vector<CreditCard*>&
VirtualCardSelectionDialogControllerImpl::GetCardList() const {
  return candidates_;
}

void VirtualCardSelectionDialogControllerImpl::OnCardSelected(
    const std::string& selected_card_id) {
  selected_card_id_ = selected_card_id;
}

void VirtualCardSelectionDialogControllerImpl::OnOkButtonClicked() {
  DCHECK(callback_);
  DCHECK(!selected_card_id_.empty());
  std::move(callback_).Run(selected_card_id_);
  // TODO(crbug.com/1020740): Add metrics.
}

void VirtualCardSelectionDialogControllerImpl::OnCancelButtonClicked() {
  // TODO(crbug.com/1020740): Add metrics.
}

void VirtualCardSelectionDialogControllerImpl::OnDialogClosed() {
  dialog_view_ = nullptr;
  callback_.Reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(VirtualCardSelectionDialogControllerImpl)

}  // namespace autofill
