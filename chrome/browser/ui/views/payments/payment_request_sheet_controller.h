// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_SHEET_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_SHEET_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/controls/button/vector_icon_button_delegate.h"

namespace views {
class Button;
class View;
}

namespace payments {

class PaymentRequestDialogView;
class PaymentRequest;

// The base class for objects responsible for the creation and event handling in
// views shown in the PaymentRequestDialog.
class PaymentRequestSheetController : public views::VectorIconButtonDelegate {
 public:
  // Objects of this class are owned by |dialog|, so it's a non-owned pointer
  // that should be valid throughout this object's lifetime.
  // |request| is also not owned by this and is guaranteed to outlive dialog.
  // Neither |request| or |dialog| should be null.
  PaymentRequestSheetController(PaymentRequest* request,
                                PaymentRequestDialogView* dialog);
  ~PaymentRequestSheetController() override {}

  virtual std::unique_ptr<views::View> CreateView() = 0;

  // The PaymentRequest object associated with this instance of the dialog.
  // Caller should not take ownership of the result.
  PaymentRequest* request() { return request_; }

  // The dialog that contains and owns this object.
  // Caller should not take ownership of the result.
  PaymentRequestDialogView* dialog() { return dialog_; }

 protected:
  // Creates and returns the primary action button for this sheet. It's
  // typically a blue button with the "Pay" or "Done" labels. Subclasses may
  // return an empty std::unique_ptr (nullptr) to indicate that no primary
  // button should be displayed. The caller takes ownership of the button but
  // the view is guaranteed to be outlived by the controller so subclasses may
  // retain a raw pointer to the returned button (for example to control its
  // enabled state).
  virtual std::unique_ptr<views::Button> CreatePrimaryButton();

  // Creates and returns the view to be displayed next to the "Pay" and "Cancel"
  // buttons. May return an empty std::unique_ptr (nullptr) to indicate that no
  // extra view is to be displayed.The caller takes ownership of the view but
  // the view is guaranteed to be outlived by the controller so subclasses may
  // retain a raw pointer to the returned view (for example to control its
  // enabled state).
  // +---------------------------+
  // | EXTRA VIEW | PAY | CANCEL |
  // +---------------------------+
  virtual std::unique_ptr<views::View> CreateExtraFooterView();

  // views::VectorIconButtonDelegate:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Creates a view to be displayed in the PaymentRequestDialog.
  // |header_view| is the view displayed on top of the dialog, containing title,
  // (optional) back button, and close buttons.
  // |content_view| is displayed between |header_view| and the pay/cancel
  // buttons. Also adds the footer, returned by CreateFooterView(), which is
  // clamped to the bottom of the containing view.  The returned view takes
  // ownership of |header_view|, |content_view|, and the footer.
  // +---------------------------+
  // |        HEADER VIEW        |
  // +---------------------------+
  // |          CONTENT          |
  // |           VIEW            |
  // +---------------------------+
  // | EXTRA VIEW | PAY | CANCEL | <-- footer
  // +---------------------------+
  std::unique_ptr<views::View> CreatePaymentView(
      std::unique_ptr<views::View> header_view,
      std::unique_ptr<views::View> content_view);

  // Creates the row of button containing the Pay, cancel, and extra buttons.
  // |controller| is installed as the listener for button events.
  std::unique_ptr<views::View> CreateFooterView();

 private:
  // Not owned. Will outlive this.
  PaymentRequest* request_;
  // Not owned. Will outlive this.
  PaymentRequestDialogView* dialog_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestSheetController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_SHEET_CONTROLLER_H_
