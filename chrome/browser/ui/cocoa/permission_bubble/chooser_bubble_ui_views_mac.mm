// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/chooser_bubble_ui.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/bubble_anchor_helper.h"
#import "chrome/browser/ui/cocoa/bubble_anchor_helper_views.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/permission_bubble/chooser_bubble_ui_cocoa.h"
#include "chrome/browser/ui/permission_bubble/chooser_bubble_delegate.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/material_design/material_design_controller.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"

// Implementation of ChooserBubbleUiView's anchor methods for Cocoa browsers. In
// Cocoa browsers there is no parent views::View for the permission bubble, so
// these methods supply an anchor point instead.

std::unique_ptr<BubbleUi> ChooserBubbleDelegate::BuildBubbleUi() {
  if (!ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    return base::MakeUnique<ChooserBubbleUiCocoa>(
        browser_, std::move(chooser_controller_));
  }
  return base::MakeUnique<ChooserBubbleUi>(browser_,
                                           std::move(chooser_controller_));
}

void ChooserBubbleUi::CreateAndShow(views::BubbleDialogDelegateView* delegate) {
  // Set |parent_window_| because some valid anchors can become hidden.
  gfx::NativeView parent =
      platform_util::GetViewForWindow(browser_->window()->GetNativeWindow());
  DCHECK(parent);
  delegate->set_parent_window(parent);
  views::BubbleDialogDelegateView::CreateBubble(delegate)->Show();
  KeepBubbleAnchored(delegate);
}

views::View* ChooserBubbleUi::GetAnchorView() {
  return nullptr;
}

gfx::Point ChooserBubbleUi::GetAnchorPoint() {
  return gfx::ScreenPointFromNSPoint(GetPermissionBubbleAnchorPointForBrowser(
      browser_, HasVisibleLocationBarForBrowser(browser_)));
}

views::BubbleBorder::Arrow ChooserBubbleUi::GetAnchorArrow() {
  return HasVisibleLocationBarForBrowser(browser_)
             ? views::BubbleBorder::TOP_LEFT
             : views::BubbleBorder::NONE;
}
