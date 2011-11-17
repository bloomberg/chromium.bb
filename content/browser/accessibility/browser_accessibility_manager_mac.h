// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_MAC_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_MAC_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "content/browser/accessibility/browser_accessibility_manager.h"

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
                                 const webkit_glue::WebAccessibility& src,
                                 BrowserAccessibilityDelegate* delegate,
                                 BrowserAccessibilityFactory* factory);

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManagerMac);
};

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_MAC_H_
