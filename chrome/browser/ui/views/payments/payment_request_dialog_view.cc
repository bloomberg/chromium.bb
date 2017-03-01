// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/payments/contact_info_view_controller.h"
#include "chrome/browser/ui/views/payments/credit_card_editor_view_controller.h"
#include "chrome/browser/ui/views/payments/order_summary_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_method_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_sheet_view_controller.h"
#include "chrome/browser/ui/views/payments/shipping_list_view_controller.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/payments/content/payment_request.h"
#include "content/public/browser/browser_thread.h"
#include "ui/views/layout/fill_layout.h"

namespace chrome {

payments::PaymentRequestDialog* CreatePaymentRequestDialog(
    payments::PaymentRequest* request) {
  return new payments::PaymentRequestDialogView(request,
                                                /* no observer */ nullptr);
}

}  // namespace chrome

namespace payments {
namespace {

// This function creates an instance of a PaymentRequestSheetController
// subclass of concrete type |Controller|, passing it non-owned pointers to
// |dialog| and the |request| that initiated that dialog. |map| should be owned
// by |dialog|.
std::unique_ptr<views::View> CreateViewAndInstallController(
    std::unique_ptr<PaymentRequestSheetController> controller,
    payments::ControllerMap* map) {
  std::unique_ptr<views::View> view = controller->CreateView();
  (*map)[view.get()] = std::move(controller);
  return view;
}

}  // namespace

PaymentRequestDialogView::PaymentRequestDialogView(
    PaymentRequest* request,
    PaymentRequestDialogView::ObserverForTest* observer)
    : request_(request), observer_for_testing_(observer), being_closed_(false) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetLayoutManager(new views::FillLayout());

  view_stack_.set_owned_by_client();
  AddChildView(&view_stack_);

  ShowInitialPaymentSheet();
}

PaymentRequestDialogView::~PaymentRequestDialogView() {}

ui::ModalType PaymentRequestDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

bool PaymentRequestDialogView::Cancel() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Called when the widget is about to close. We send a message to the
  // PaymentRequest object to signal user cancellation. Before destroying the
  // PaymentRequest object, we destroy all controllers so that they are not left
  // alive with an invalid PaymentRequest pointer.
  being_closed_ = true;
  controller_map_.clear();
  request_->UserCancelled();
  return true;
}

bool PaymentRequestDialogView::ShouldShowCloseButton() const {
  // Don't show the normal close button on the dialog. This is because the
  // typical dialog header doesn't allow displaying anything other that the
  // title and the close button. This is insufficient for the PaymentRequest
  // dialog, which must sometimes show the back arrow next to the title.
  // Moreover, the title (and back arrow) should animate with the view they're
  // attached to.
  return false;
}

int PaymentRequestDialogView::GetDialogButtons() const {
  // The buttons should animate along with the different dialog sheets since
  // each sheet presents a different set of buttons. Because of this, hide the
  // usual dialog buttons.
  return ui::DIALOG_BUTTON_NONE;
}

void PaymentRequestDialogView::GoBack() {
  view_stack_.Pop();

  if (observer_for_testing_)
    observer_for_testing_->OnBackNavigation();
}

void PaymentRequestDialogView::ShowContactInfoSheet() {
  view_stack_.Push(
      CreateViewAndInstallController(
          base::MakeUnique<ContactInfoViewController>(request_, this),
          &controller_map_),
      /* animate */ true);
  if (observer_for_testing_)
    observer_for_testing_->OnContactInfoOpened();
}

void PaymentRequestDialogView::ShowOrderSummary() {
  view_stack_.Push(
      CreateViewAndInstallController(
          base::MakeUnique<OrderSummaryViewController>(request_, this),
          &controller_map_),
      /* animate = */ true);
  if (observer_for_testing_)
    observer_for_testing_->OnOrderSummaryOpened();
}

void PaymentRequestDialogView::ShowPaymentMethodSheet() {
  view_stack_.Push(
      CreateViewAndInstallController(
          base::MakeUnique<PaymentMethodViewController>(request_, this),
          &controller_map_),
      /* animate = */ true);
  if (observer_for_testing_)
    observer_for_testing_->OnPaymentMethodOpened();
}

void PaymentRequestDialogView::ShowShippingListSheet() {
  view_stack_.Push(
      CreateViewAndInstallController(
          base::MakeUnique<ShippingListViewController>(request_, this),
          &controller_map_),
      /* animate = */ true);
}

void PaymentRequestDialogView::ShowCreditCardEditor() {
  view_stack_.Push(
      CreateViewAndInstallController(
          base::MakeUnique<CreditCardEditorViewController>(request_, this),
          &controller_map_),
      /* animate = */ true);
  if (observer_for_testing_)
    observer_for_testing_->OnCreditCardEditorOpened();
}

void PaymentRequestDialogView::ShowDialog() {
  constrained_window::ShowWebModalDialogViews(this, request_->web_contents());
}

void PaymentRequestDialogView::CloseDialog() {
  // This calls PaymentRequestDialogView::Cancel() before closing.
  // ViewHierarchyChanged() also gets called after Cancel().
  GetWidget()->Close();
}

void PaymentRequestDialogView::ShowInitialPaymentSheet() {
  view_stack_.Push(
      CreateViewAndInstallController(
          base::MakeUnique<PaymentSheetViewController>(request_, this),
          &controller_map_),
      /* animate = */ false);
  if (observer_for_testing_)
    observer_for_testing_->OnDialogOpened();
}

gfx::Size PaymentRequestDialogView::GetPreferredSize() const {
  return gfx::Size(450, 450);
}

void PaymentRequestDialogView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (being_closed_)
    return;

  // When a view that is associated with a controller is removed from this
  // view's descendants, dispose of the controller.
  if (!details.is_add &&
      controller_map_.find(details.child) != controller_map_.end()) {
    DCHECK(!details.move_view);
    controller_map_.erase(details.child);
  }
}

}  // namespace payments
