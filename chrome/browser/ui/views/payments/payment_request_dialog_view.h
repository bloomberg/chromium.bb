// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/payments/view_stack.h"
#include "components/payments/content/payment_request_dialog.h"
#include "ui/views/window/dialog_delegate.h"

namespace payments {

class PaymentRequest;
class PaymentRequestSheetController;

// Maps views owned by PaymentRequestDialogView::view_stack_ to their
// controller. PaymentRequestDialogView is responsible for listening for those
// views being removed from the hierarchy and delete the associated controllers.
using ControllerMap =
    std::map<views::View*, std::unique_ptr<PaymentRequestSheetController>>;

// The dialog delegate that represents a desktop WebPayments dialog. This class
// is responsible for displaying the view associated with the current state of
// the WebPayments flow and managing the transition between those states.
class PaymentRequestDialogView : public views::DialogDelegateView,
                                 public PaymentRequestDialog {
 public:
  class ObserverForTest {
   public:
    virtual void OnDialogOpened() = 0;

    virtual void OnContactInfoOpened() = 0;

    virtual void OnOrderSummaryOpened() = 0;

    virtual void OnPaymentMethodOpened() = 0;

    virtual void OnCreditCardEditorOpened() = 0;

    virtual void OnBackNavigation() = 0;
  };

  // Build a Dialog around the PaymentRequest object. |observer| is used to
  // be notified of dialog events as they happen (but may be NULL) and should
  // outlive this object.
  PaymentRequestDialogView(PaymentRequest* request,
                           PaymentRequestDialogView::ObserverForTest* observer);
  ~PaymentRequestDialogView() override;

  // views::WidgetDelegate
  ui::ModalType GetModalType() const override;

  // views::DialogDelegate
  bool Cancel() override;
  bool ShouldShowCloseButton() const override;
  int GetDialogButtons() const override;

  // payments::PaymentRequestDialog
  void ShowDialog() override;
  void CloseDialog() override;

  void GoBack();
  void ShowContactInfoSheet();
  void ShowOrderSummary();
  void ShowShippingListSheet();
  void ShowPaymentMethodSheet();
  void ShowCreditCardEditor();

  ViewStack* view_stack_for_testing() { return &view_stack_; }

 private:
  void ShowInitialPaymentSheet();

  // views::View
  gfx::Size GetPreferredSize() const override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

  // Non-owned reference to the PaymentRequest that initiated this dialog. Since
  // the PaymentRequest object always outlives this one, the pointer should
  // always be valid even though there is no direct ownership relationship
  // between the two.
  PaymentRequest* request_;
  ControllerMap controller_map_;
  ViewStack view_stack_;

  // May be null.
  ObserverForTest* observer_for_testing_;

  // Used when the dialog is being closed to avoid re-entrancy into the
  // controller_map_.
  bool being_closed_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestDialogView);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_VIEW_H_
