// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/accessibility/browser_accessibility_mac.h"

#import "chrome/browser/accessibility/browser_accessibility_cocoa.h"
#import "chrome/browser/accessibility/browser_accessibility_delegate_mac.h"
#include "chrome/browser/accessibility/browser_accessibility_manager.h"
#import "chrome/browser/renderer_host/render_widget_host_view_mac.h"


// Static.
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibilityMac();
}

BrowserAccessibilityMac::BrowserAccessibilityMac()
    : browser_accessibility_cocoa_(NULL) {
}

void BrowserAccessibilityMac::Initialize() {
  BrowserAccessibility::Initialize();

  if (browser_accessibility_cocoa_)
    return;

  // We take ownership of the cocoa obj here.
  browser_accessibility_cocoa_ = [[BrowserAccessibilityCocoa alloc]
      initWithObject:this
      delegate:(RenderWidgetHostViewCocoa*)manager_->GetParentView()];
}

void BrowserAccessibilityMac::NativeReleaseReference() {
  if (browser_accessibility_cocoa_) {
    BrowserAccessibilityCocoa* temp = browser_accessibility_cocoa_;
    browser_accessibility_cocoa_ = nil;
    // Relinquish ownership of the cocoa obj.
    [temp release];
    // At this point, other processes may have a reference to
    // the cocoa object. When the retain count hits zero, it will
    // destroy us in dealloc.
    // For that reason, do *not* make any more calls here after
    // as we might have been deleted.
  }
}

void BrowserAccessibilityMac::ReplaceChild(
      BrowserAccessibility* old_acc,
      BrowserAccessibility* new_acc) {
  BrowserAccessibility::ReplaceChild(old_acc, new_acc);
  [browser_accessibility_cocoa_ childrenChanged];
}

BrowserAccessibilityCocoa* BrowserAccessibility::toBrowserAccessibilityCocoa() {
  return static_cast<BrowserAccessibilityMac*>(this)->
      native_view();
}
