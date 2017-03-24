// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_SHEET_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_SHEET_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/controls/button/button.h"

namespace views {
class View;
}

namespace payments {

class PaymentRequestDialogView;
class PaymentRequestSpec;
class PaymentRequestState;

// The base class for objects responsible for the creation and event handling in
// views shown in the PaymentRequestDialog.
class PaymentRequestSheetController : public views::ButtonListener {
 public:
  // Objects of this class are owned by |dialog|, so it's a non-owned pointer
  // that should be valid throughout this object's lifetime.
  // |state| and |spec| are also not owned by this and are guaranteed to outlive
  // dialog. Neither |state|, |spec| or |dialog| should be null.
  PaymentRequestSheetController(PaymentRequestSpec* spec,
                                PaymentRequestState* state,
                                PaymentRequestDialogView* dialog);
  ~PaymentRequestSheetController() override;

  std::unique_ptr<views::View> CreateView();

  PaymentRequestSpec* spec() { return spec_; }
  PaymentRequestState* state() { return state_; }

  // The dialog that contains and owns this object.
  // Caller should not take ownership of the result.
  PaymentRequestDialogView* dialog() { return dialog_; }

 protected:
  // Clears the content part of the view represented by this view controller and
  // calls FillContentView again to re-populate it with updated views.
  void UpdateContentView();

  // Creates and returns the primary action button for this sheet. It's
  // typically a blue button with the "Pay" or "Done" labels. Subclasses may
  // return an empty std::unique_ptr (nullptr) to indicate that no primary
  // button should be displayed. The caller takes ownership of the button but
  // the view is guaranteed to be outlived by the controller so subclasses may
  // retain a raw pointer to the returned button (for example to control its
  // enabled state).
  virtual std::unique_ptr<views::Button> CreatePrimaryButton();

  // Returns whether this sheet should display a back arrow in the header next
  // to the title.
  virtual bool ShouldShowHeaderBackArrow();

  // Returns the title to be displayed in this sheet's header.
  virtual base::string16 GetSheetTitle() = 0;

  // Implemented by subclasses to populate |content_view| with the views that
  // should be displayed in their content area (between the header and the
  // footer). This may be called at view creation time as well as anytime
  // UpdateContentView is called.
  virtual void FillContentView(views::View* content_view) = 0;

  // Creates and returns the view to be displayed next to the "Pay" and "Cancel"
  // buttons. May return an empty std::unique_ptr (nullptr) to indicate that no
  // extra view is to be displayed.The caller takes ownership of the view but
  // the view is guaranteed to be outlived by the controller so subclasses may
  // retain a raw pointer to the returned view (for example to control its
  // enabled state). The horizontal and vertical insets (to the left and bottom
  // borders) is taken care of by the caller, so can be set to 0.
  // +---------------------------+
  // | EXTRA VIEW | PAY | CANCEL |
  // +---------------------------+
  virtual std::unique_ptr<views::View> CreateExtraFooterView();

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Creates the row of button containing the Pay, cancel, and extra buttons.
  // |controller| is installed as the listener for button events.
  std::unique_ptr<views::View> CreateFooterView();

 private:
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
  std::unique_ptr<views::View> CreatePaymentView();

  // All these are not owned. Will outlive this.
  PaymentRequestSpec* spec_;
  PaymentRequestState* state_;
  PaymentRequestDialogView* dialog_;

  // This view is owned by its encompassing ScrollView.
  views::View* content_view_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestSheetController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_SHEET_CONTROLLER_H_
