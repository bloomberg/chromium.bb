// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_GENERATED_CREDIT_CARD_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_GENERATED_CREDIT_CARD_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/autofill/generated_credit_card_bubble_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_decoration_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/views/controls/image_view.h"

namespace autofill {
class GeneratedCreditCardBubbleController;
}

////////////////////////////////////////////////////////////////////////////////
//
// GeneratedCreditCardView
//
//  An icon that shows up in the omnibox after successfully generating a credit
//  card number. Used as an anchor and click target to show the associated
//  bubble with more details about the credit cards saved or used.
//
////////////////////////////////////////////////////////////////////////////////
class GeneratedCreditCardView : public LocationBarDecorationView {
 public:
  explicit GeneratedCreditCardView(LocationBarView::Delegate* delegate);
  virtual ~GeneratedCreditCardView();

  void Update();

 protected:
  // LocationBarDecorationView:
  virtual bool CanHandleClick() const OVERRIDE;
  virtual void OnClick() OVERRIDE;

 private:
  // Helper to get the GeneratedCreditCardBubbleController associated with the
  // current web contents.
  autofill::GeneratedCreditCardBubbleController* GetController() const;

  LocationBarView::Delegate* delegate_;  // weak; outlives us.

  DISALLOW_COPY_AND_ASSIGN(GeneratedCreditCardView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_GENERATED_CREDIT_CARD_VIEW_H_
