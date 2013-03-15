// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_MAC_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_MAC_H_

#import <Cocoa/Cocoa.h>

#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {

class CONTENT_EXPORT BrowserAccessibilityManagerMac
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerMac(
      NSView* parent_view,
      const AccessibilityNodeData& src,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory = new BrowserAccessibilityFactory());

  static AccessibilityNodeData GetEmptyDocument();

  // Implementation of BrowserAccessibilityManager.
  virtual void NotifyAccessibilityEvent(int type,
                                        BrowserAccessibility* node) OVERRIDE;

  NSView* parent_view() { return parent_view_; }

 private:
  // This gives BrowserAccessibilityManager::Create access to the class
  // constructor.
  friend class BrowserAccessibilityManager;

  NSView* parent_view_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManagerMac);
};

}

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_MAC_H_
