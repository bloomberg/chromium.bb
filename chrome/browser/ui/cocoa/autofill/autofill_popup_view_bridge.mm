// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/cocoa/autofill/autofill_popup_view_bridge.h"

#include "base/logging.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_popup_view_cocoa.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/gfx/rect.h"

AutofillPopupViewBridge::AutofillPopupViewBridge(
    AutofillPopupController* controller)
    : controller_(controller) {
  window_ =
      [[NSWindow alloc] initWithContentRect:ui::kWindowSizeDeterminedLater
                                  styleMask:NSBorderlessWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:YES];
  // Telling Cocoa that the window is opaque enables some drawing optimizations.
  [window_ setOpaque:YES];

  view_ = [[[AutofillPopupViewCocoa alloc]
             initWithController:controller_
                          frame:[[window_ contentView] frame]] autorelease];
  [window_ setContentView:view_];
}

AutofillPopupViewBridge::~AutofillPopupViewBridge() {
  [view_ controllerDestroyed];
  controller_->ViewDestroyed();

  [window_ close];
}

void AutofillPopupViewBridge::Hide() {
  delete this;
}

void AutofillPopupViewBridge::Show() {
  SetInitialBounds();
  UpdateBoundsAndRedrawPopup();
  [[controller_->container_view() window] addChildWindow:window_
                                                 ordered:NSWindowAbove];
}

void AutofillPopupViewBridge::InvalidateRow(size_t row) {
  NSRect dirty_rect =
      NSRectFromCGRect(controller_->GetRowBounds(row).ToCGRect());
  [view_ setNeedsDisplayInRect:dirty_rect];
}

void AutofillPopupViewBridge::UpdateBoundsAndRedrawPopup() {
  NSRect frame = NSRectFromCGRect(controller_->popup_bounds().ToCGRect());

  // Flip coordinates back into Cocoa-land.
  // TODO(isherman): Does this agree with the controller's idea of handling
  // multi-monitor setups correctly?
  NSScreen* screen = [[controller_->container_view() window] screen];
  frame.origin.y = NSHeight([screen frame]) - NSMaxY(frame);

  // TODO(isherman): The view should support scrolling if the popup gets too
  // big to fit on the screen.
  [window_ setFrame:frame display:YES];
}

void AutofillPopupViewBridge::SetInitialBounds() {
  // The bounds rect in Chrome's screen coordinates.  The popup should be
  // positioned just below the element which initiated it.
  gfx::Rect bounds(controller_->element_bounds().x(),
                   controller_->element_bounds().bottom(),
                   controller_->GetPopupRequiredWidth(),
                   controller_->GetPopupRequiredHeight());

  // TODO(isherman): Position the popup correctly if it can't fit below the text
  // field: http://crbug.com/164603

  controller_->SetPopupBounds(bounds);
}

AutofillPopupView* AutofillPopupView::Create(
    AutofillPopupController* controller) {
  return new AutofillPopupViewBridge(controller);
}
