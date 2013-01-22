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

namespace {

// The width of the border around the popup.
const CGFloat kBorderWidth = 1.0;

// The color of the border around the popup.
NSColor* BorderColor() {
  return [NSColor colorForControlTint:[NSColor currentControlTint]];
}

// Returns a view that contains a border around the content view.
NSBox* CreateBorderView() {
  // TODO(isherman): We should consider using asset-based drawing for the
  // border, creating simple bitmaps for the view's border and background, and
  // drawing them using NSDrawNinePartImage().
  NSBox* border_view = [[[NSBox alloc] initWithFrame:NSZeroRect] autorelease];
  [border_view setBorderColor:BorderColor()];
  [border_view setBorderType:NSLineBorder];
  [border_view setBorderWidth:kBorderWidth];
  [border_view setBoxType:NSBoxCustom];
  [border_view setContentViewMargins:NSZeroSize];
  [border_view setTitlePosition:NSNoTitle];
  return border_view;
}

}

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

  NSBox* border_view = CreateBorderView();
  [window_ setContentView:border_view];

  view_ = [[[AutofillPopupViewCocoa alloc]
             initWithController:controller_
                          frame:NSZeroRect] autorelease];
  [border_view setContentView:view_];
}

AutofillPopupViewBridge::~AutofillPopupViewBridge() {
  [view_ controllerDestroyed];
  controller_->ViewDestroyed();

  // Remove the child window before closing, otherwise it can mess up
  // display ordering.
  [[window_ parentWindow] removeChildWindow:window_];

  [window_ close];
}

void AutofillPopupViewBridge::Hide() {
  delete this;
}

void AutofillPopupViewBridge::Show() {
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

  // Leave room for the border.
  frame = NSInsetRect(frame, -kBorderWidth, -kBorderWidth);
  if (controller_->popup_bounds().y() > controller_->element_bounds().y()) {
    // Popup is below the element which initiated it.
    frame.origin.y -= kBorderWidth;
  } else {
    // Popup is above the element which initiated it.
    frame.origin.y += kBorderWidth;
  }

  // TODO(isherman): The view should support scrolling if the popup gets too
  // big to fit on the screen.
  [window_ setFrame:frame display:YES];
}

AutofillPopupView* AutofillPopupView::Create(
    AutofillPopupController* controller) {
  return new AutofillPopupViewBridge(controller);
}
