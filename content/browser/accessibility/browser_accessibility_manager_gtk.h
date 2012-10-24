// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_GTK_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_GTK_H_

#include "content/browser/accessibility/browser_accessibility_manager.h"

struct ViewHostMsg_AccessibilityNotification_Params;

namespace content {
class BrowserAccessibilityGtk;

// Manages a tree of BrowserAccessibilityGtk objects.
class BrowserAccessibilityManagerGtk : public BrowserAccessibilityManager {
 public:
  virtual ~BrowserAccessibilityManagerGtk();

  // BrowserAccessibilityManager methods
  virtual void NotifyAccessibilityEvent(int type, BrowserAccessibility* node)
      OVERRIDE;

 private:
  BrowserAccessibilityManagerGtk(
      GtkWidget* parent_window,
      const AccessibilityNodeData& src,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory);

  void RecursivelySendChildrenChanged(BrowserAccessibilityGtk* node);

  // Give BrowserAccessibilityManager::Create access to our constructor.
  friend class BrowserAccessibilityManager;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManagerGtk);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_GTK_H_
