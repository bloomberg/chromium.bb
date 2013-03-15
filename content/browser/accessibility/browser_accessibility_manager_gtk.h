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
class CONTENT_EXPORT BrowserAccessibilityManagerGtk
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerGtk(
      GtkWidget* parent_widget,
      const AccessibilityNodeData& src,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory = new BrowserAccessibilityFactory());

  virtual ~BrowserAccessibilityManagerGtk();

  static AccessibilityNodeData GetEmptyDocument();

  // BrowserAccessibilityManager methods
  virtual void NotifyAccessibilityEvent(int type, BrowserAccessibility* node)
      OVERRIDE;

  GtkWidget* parent_widget() { return parent_widget_; }

 private:
  void RecursivelySendChildrenChanged(BrowserAccessibilityGtk* node);

  GtkWidget* parent_widget_;

  // Give BrowserAccessibilityManager::Create access to our constructor.
  friend class BrowserAccessibilityManager;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManagerGtk);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_GTK_H_
