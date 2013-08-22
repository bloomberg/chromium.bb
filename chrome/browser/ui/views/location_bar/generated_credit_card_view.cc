// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/generated_credit_card_view.h"

#include "chrome/browser/ui/autofill/generated_credit_card_bubble_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "ui/gfx/image/image.h"

GeneratedCreditCardView::GeneratedCreditCardView(
    LocationBarView::Delegate* delegate)
    : delegate_(delegate) {
  Update();
}

GeneratedCreditCardView::~GeneratedCreditCardView() {}

void GeneratedCreditCardView::Update() {
  autofill::GeneratedCreditCardBubbleController* controller = GetController();
  if (controller && !controller->AnchorIcon().IsEmpty()) {
    SetVisible(true);
    SetImage(controller->AnchorIcon().AsImageSkia());
  } else {
    SetVisible(false);
    SetImage(NULL);
  }
}

// TODO(dbeam): figure out what to do for a tooltip and accessibility.

bool GeneratedCreditCardView::CanHandleClick() const {
  autofill::GeneratedCreditCardBubbleController* controller = GetController();
  return controller && !controller->IsHiding();
}

void GeneratedCreditCardView::OnClick() {
  autofill::GeneratedCreditCardBubbleController* controller = GetController();
  if (controller)
    controller->OnAnchorClicked();
}

autofill::GeneratedCreditCardBubbleController* GeneratedCreditCardView::
    GetController() const {
  content::WebContents* wc = delegate_->GetWebContents();
  if (!wc || delegate_->GetToolbarModel()->input_in_progress())
    return NULL;

  return autofill::GeneratedCreditCardBubbleController::FromWebContents(wc);
}
