// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_AUTOFILL_CREDIT_CARD_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_AUTOFILL_CREDIT_CARD_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/autofill/autofill_credit_card_bubble_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_decoration_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/views/controls/image_view.h"

class ToolbarModel;

namespace autofill {
class AutofillCreditCardBubbleController;
}

////////////////////////////////////////////////////////////////////////////////
//
// AutofillCreditCardView
//
//  An icon that shows up in the omnibox after successfully submitting the
//  Autofill dialog. Used as an anchor and click target to show the associated
//  bubble with more details about the credit cards saved or used.
//
////////////////////////////////////////////////////////////////////////////////
class AutofillCreditCardView : public LocationBarDecorationView {
 public:
  AutofillCreditCardView(ToolbarModel* toolbar_model,
                         LocationBarView::Delegate* delegate);
  virtual ~AutofillCreditCardView();

  void Update();

 protected:
  // LocationBarDecorationView:
  virtual bool CanHandleClick() const OVERRIDE;
  virtual void OnClick() OVERRIDE;

 private:
  // Helper to get the AutofillCreditCardBubbleController associated with the
  // current web contents.
  autofill::AutofillCreditCardBubbleController* GetController() const;

  ToolbarModel* toolbar_model_;  // weak; outlives us.
  LocationBarView::Delegate* delegate_;  // weak; outlives us.

  DISALLOW_COPY_AND_ASSIGN(AutofillCreditCardView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_AUTOFILL_CREDIT_CARD_VIEW_H_
