// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/accessibility/browser_accessibility_mac.h"

// Static.
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibilityMac();
}

BrowserAccessibilityMac::BrowserAccessibilityMac() {
}

// TODO(dtseng): ensure we create BrowserAccessibilityCocoa here
// (RenderWidgetHostViewCocoa to BrowserAccessibilityManagerMac refactoring).
void BrowserAccessibilityMac::Initialize() {
}

// TODO(dtseng): ensure we cleanup BrowserAccessibilityCocoa and this class.
// (RenderWidgetHostViewCocoa to BrowserAccessibilityManagerMac refactoring).
void BrowserAccessibilityMac::ReleaseReference() {
}

BrowserAccessibilityMac* BrowserAccessibility::toBrowserAccessibilityMac() {
  return static_cast<BrowserAccessibilityMac*>(this);
}
