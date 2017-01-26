// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/payments/order_summary_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_method_view_controller.h"
#include "chrome/browser/ui/views/payments/payment_sheet_view_controller.h"
#include "chrome/browser/ui/views/payments/shipping_list_view_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/payments/payment_request.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
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
template <typename Controller>
std::unique_ptr<views::View> CreateViewAndInstallController(
    payments::ControllerMap* map,
    payments::PaymentRequest* request,
    payments::PaymentRequestDialogView* dialog) {
  std::unique_ptr<Controller> controller =
      base::MakeUnique<Controller>(request, dialog);
  std::unique_ptr<views::View> view = controller->CreateView();
  (*map)[view.get()] = std::move(controller);
  return view;
}

}  // namespace

PaymentRequestDialogView::PaymentRequestDialogView(
    PaymentRequest* request,
    PaymentRequestDialogView::ObserverForTest* observer)
    : request_(request), observer_for_testing_(observer) {
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
  // Called when the widget is about to close. We send a message to the
  // PaymentRequest object to signal user cancellation.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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
}

void PaymentRequestDialogView::ShowOrderSummary() {
  view_stack_.Push(CreateViewAndInstallController<OrderSummaryViewController>(
                       &controller_map_, request_, this),
                   /* animate = */ true);
}

void PaymentRequestDialogView::ShowPaymentMethodSheet() {
  view_stack_.Push(CreateViewAndInstallController<PaymentMethodViewController>(
                       &controller_map_, request_, this),
                   /* animate = */ true);
}

void PaymentRequestDialogView::ShowShippingListSheet() {
  view_stack_.Push(CreateViewAndInstallController<ShippingListViewController>(
                       &controller_map_, request_, this),
                   /* animate = */ true);
}

void PaymentRequestDialogView::ShowDialog() {
  constrained_window::ShowWebModalDialogViews(this, request_->web_contents());
}

void PaymentRequestDialogView::CloseDialog() {
  // This calls PaymentRequestDialogView::Cancel() before closing.
  GetWidget()->Close();
}

void PaymentRequestDialogView::ShowInitialPaymentSheet() {
  view_stack_.Push(CreateViewAndInstallController<PaymentSheetViewController>(
                       &controller_map_, request_, this),
                   /* animate = */ false);
  if (observer_for_testing_)
    observer_for_testing_->OnDialogOpened();
}

gfx::Size PaymentRequestDialogView::GetPreferredSize() const {
  return gfx::Size(450, 450);
}

void PaymentRequestDialogView::ViewHierarchyChanged(
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
