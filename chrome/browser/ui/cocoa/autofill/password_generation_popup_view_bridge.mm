// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/cocoa/autofill/password_generation_popup_view_bridge.h"

#include "base/logging.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#import "chrome/browser/ui/cocoa/autofill/password_generation_popup_view_cocoa.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/gfx/rect.h"

namespace autofill {

PasswordGenerationPopupViewBridge::PasswordGenerationPopupViewBridge(
    PasswordGenerationPopupController* controller) {
  view_.reset(
      [[PasswordGenerationPopupViewCocoa alloc]
          initWithController:controller
                       frame:NSZeroRect]);
}

PasswordGenerationPopupViewBridge::~PasswordGenerationPopupViewBridge() {
  [view_ controllerDestroyed];
  [view_ hidePopup];
}

void PasswordGenerationPopupViewBridge::Hide() {
  delete this;
}

void PasswordGenerationPopupViewBridge::Show() {
  [view_ showPopup];
}

gfx::Size PasswordGenerationPopupViewBridge::GetPreferredSizeOfPasswordView() {
  // TODO(gcasto): Implement this function.
  return gfx::Size();
}

void PasswordGenerationPopupViewBridge::UpdateBoundsAndRedrawPopup() {
  [view_ updateBoundsAndRedrawPopup];
}

void PasswordGenerationPopupViewBridge::PasswordSelectionUpdated() {
  [view_ setNeedsDisplay:YES];
}

bool PasswordGenerationPopupViewBridge::IsPointInPasswordBounds(
    const gfx::Point& point) {
  // TODO(gcasto): Implement this function.
  return true;
}

PasswordGenerationPopupView* PasswordGenerationPopupView::Create(
    PasswordGenerationPopupController* controller) {
  return new PasswordGenerationPopupViewBridge(controller);
}

}  // namespace autofill
