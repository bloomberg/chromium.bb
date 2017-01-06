// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_dialog.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/payments/payment_request_impl.h"
#include "chrome/browser/ui/views/payments/order_summary_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_sheet_view_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/fill_layout.h"

namespace chrome {

void ShowPaymentRequestDialog(payments::PaymentRequestImpl* impl) {
  constrained_window::ShowWebModalDialogViews(
      new payments::PaymentRequestDialog(impl), impl->web_contents());
}

}  // namespace chrome

namespace payments {
namespace {

// This function creates an instance of a PaymentRequestSheetController
// subclass of concrete type |Controller|, passing it non-owned pointers to
// |dialog| and the |impl| that initiated that dialog. |map| should be owned by
// |dialog|.
template<typename Controller>
std::unique_ptr<views::View> CreateViewAndInstallController(
    payments::ControllerMap* map,
    payments::PaymentRequestImpl* impl,
    payments::PaymentRequestDialog* dialog) {
  std::unique_ptr<Controller> controller =
      base::MakeUnique<Controller>(impl, dialog);
  std::unique_ptr<views::View> view = controller->CreateView();
  (*map)[view.get()] = std::move(controller);
  return view;
}

}  // namespace

PaymentRequestDialog::PaymentRequestDialog(PaymentRequestImpl* impl)
    : impl_(impl) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetLayoutManager(new views::FillLayout());

  view_stack_.set_owned_by_client();
  AddChildView(&view_stack_);

  ShowInitialPaymentSheet();
}

PaymentRequestDialog::~PaymentRequestDialog() {}

ui::ModalType PaymentRequestDialog::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

bool PaymentRequestDialog::Cancel() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  impl_->Cancel();
  return true;
}

bool PaymentRequestDialog::ShouldShowCloseButton() const {
  // Don't show the normal close button on the dialog. This is because the
  // typical dialog header doesn't allow displaying anything other that the
  // title and the close button. This is insufficient for the PaymentRequest
  // dialog, which must sometimes show the back arrow next to the title.
  // Moreover, the title (and back arrow) should animate with the view they're
  // attached to.
  return false;
}

int PaymentRequestDialog::GetDialogButtons() const {
  // The buttons should animate along with the different dialog sheets since
  // each sheet presents a different set of buttons. Because of this, hide the
  // usual dialog buttons.
  return ui::DIALOG_BUTTON_NONE;
}

void PaymentRequestDialog::GoBack() {
  view_stack_.Pop();
}

void PaymentRequestDialog::ShowOrderSummary() {
    view_stack_.Push(
        CreateViewAndInstallController<OrderSummaryViewController>(
            &controller_map_, impl_, this),
        true);
}

void PaymentRequestDialog::CloseDialog() {
  GetWidget()->Close();
}

void PaymentRequestDialog::ShowInitialPaymentSheet() {
  view_stack_.Push(
      CreateViewAndInstallController<PaymentSheetViewController>(
          &controller_map_, impl_, this),
      false);
}

gfx::Size PaymentRequestDialog::GetPreferredSize() const {
  return gfx::Size(450, 450);
}

void PaymentRequestDialog::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  // When a view that is associated with a controller is removed from this
  // view's descendants, dispose of the controller.
  if (!details.is_add &&
      controller_map_.find(details.child) != controller_map_.end()) {
    DCHECK(!details.move_view);
    controller_map_.erase(details.child);
  }
}

}  // namespace payments
