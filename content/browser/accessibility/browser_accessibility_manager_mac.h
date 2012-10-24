// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_MAC_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_MAC_H_

#import <Cocoa/Cocoa.h>

#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {

class BrowserAccessibilityManagerMac : public BrowserAccessibilityManager {
 public:
  // Implementation of BrowserAccessibilityManager.
  virtual void NotifyAccessibilityEvent(int type,
                                        BrowserAccessibility* node) OVERRIDE;

 private:
  // This gives BrowserAccessibilityManager::Create access to the class
  // constructor.
  friend class BrowserAccessibilityManager;

  BrowserAccessibilityManagerMac(gfx::NativeView parent_view,
                                 const AccessibilityNodeData& src,
                                 BrowserAccessibilityDelegate* delegate,
                                 BrowserAccessibilityFactory* factory);

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManagerMac);
};

}

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_MAC_H_
