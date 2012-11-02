// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_DIALOG_VIEWS_H_

#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

class ConstrainedWindowViews;

namespace views {
class Checkbox;
}

namespace autofill {

class AutofillDialogController;
struct DetailInput;

// Views toolkit implementation of the Autofill dialog that handles the
// imperative autocomplete API call.
class AutofillDialogViews : public AutofillDialogView,
                            public views::DialogDelegate,
                            public views::ButtonListener {
 public:
  explicit AutofillDialogViews(AutofillDialogController* controller);
  virtual ~AutofillDialogViews();

  // AutofillDialogView implementation:
  virtual void Show() OVERRIDE;

  // views::DialogDelegate implementation:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const OVERRIDE;
  virtual bool UseChromeStyle() const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

 private:
  void InitChildViews();

  // Creates and returns a view that holds all detail sections.
  views::View* CreateDetailsContainer();

  // These functions create the views that hold inputs for the section.
  views::View* CreateEmailInputs();
  views::View* CreateBillingInputs();
  views::View* CreateShippingInputs();

  // Reads a DetailInput array and creates inputs in a grid.
  views::View* InitInputsFromTemplate(const DetailInput* inputs,
                                      size_t inputs_len);

  // The controller that drives this view. Weak pointer, always non-NULL.
  AutofillDialogController* const controller_;

  // The window that displays |contents_|. Weak pointer; may be NULL when the
  // dialog is closing.
  ConstrainedWindowViews* window_;

  // The top-level View for the dialog. Owned by the constrained window.
  views::View* contents_;

  // The checkbox that controls whether to use the billing details for shipping
  // as well.
  views::Checkbox* use_billing_for_shipping_;

  // The shipping section. May not be visible.
  views::View* shipping_section_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_DIALOG_VIEWS_H_
