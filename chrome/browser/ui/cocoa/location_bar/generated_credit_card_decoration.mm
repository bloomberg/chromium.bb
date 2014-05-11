// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/location_bar/generated_credit_card_decoration.h"

#include "chrome/browser/ui/autofill/generated_credit_card_bubble_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "grit/theme_resources.h"

using autofill::GeneratedCreditCardBubbleController;

GeneratedCreditCardDecoration::GeneratedCreditCardDecoration(
    LocationBarViewMac* owner) : owner_(owner) {
}

GeneratedCreditCardDecoration::~GeneratedCreditCardDecoration() {
}

void GeneratedCreditCardDecoration::Update() {
  GeneratedCreditCardBubbleController* controller = GetController();
  if (controller && !controller->AnchorIcon().IsEmpty()) {
    SetVisible(true);
    SetImage(controller->AnchorIcon().AsNSImage());
  } else {
    SetVisible(false);
    SetImage(nil);
  }
}

NSPoint GeneratedCreditCardDecoration::GetBubblePointInFrame(NSRect frame) {
  const NSRect draw_frame = GetDrawRectInFrame(frame);
  return NSMakePoint(NSMidX(draw_frame), NSMaxY(draw_frame));
}

bool GeneratedCreditCardDecoration::AcceptsMousePress() {
  GeneratedCreditCardBubbleController* controller = GetController();
  return controller && !controller->IsHiding();
}

bool GeneratedCreditCardDecoration::OnMousePressed(NSRect frame,
                                                   NSPoint location) {
  GeneratedCreditCardBubbleController* controller = GetController();
  if (!controller)
    return false;

  controller->OnAnchorClicked();
  return true;
}

GeneratedCreditCardBubbleController* GeneratedCreditCardDecoration::
    GetController() const {
  content::WebContents* wc = owner_->GetWebContents();
  if (!wc || owner_->GetToolbarModel()->input_in_progress()) {
    return NULL;
  }

  GeneratedCreditCardBubbleController* controller =
      GeneratedCreditCardBubbleController::FromWebContents(wc);
  return controller;
}

