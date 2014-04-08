// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_

#include <oleacc.h>

#include "base/memory/scoped_ptr.h"
#include "base/win/scoped_comptr.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {
class BrowserAccessibilityWin;

class LegacyRenderWidgetHostHWND;

// Manages a tree of BrowserAccessibilityWin objects.
class CONTENT_EXPORT BrowserAccessibilityManagerWin
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerWin(
      content::LegacyRenderWidgetHostHWND* accessible_hwnd,
      IAccessible* parent_iaccessible,
      const ui::AXNodeData& src,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory = new BrowserAccessibilityFactory());

  virtual ~BrowserAccessibilityManagerWin();

  static ui::AXNodeData GetEmptyDocument();

  // Get the closest containing HWND.
  HWND parent_hwnd() { return parent_hwnd_; }

  // The IAccessible for the parent window.
  IAccessible* parent_iaccessible() { return parent_iaccessible_; }
  void set_parent_iaccessible(IAccessible* parent_iaccessible) {
    parent_iaccessible_ = parent_iaccessible;
  }

  // Calls NotifyWinEvent if the parent window's IAccessible pointer is known.
  void MaybeCallNotifyWinEvent(DWORD event, LONG child_id);

  // BrowserAccessibilityManager methods
  virtual void AddNodeToMap(BrowserAccessibility* node);
  virtual void RemoveNode(BrowserAccessibility* node) OVERRIDE;
  virtual void OnWindowFocused() OVERRIDE;
  virtual void OnWindowBlurred() OVERRIDE;
  virtual void NotifyAccessibilityEvent(
      ui::AXEvent event_type, BrowserAccessibility* node) OVERRIDE;

  // Track this object and post a VISIBLE_DATA_CHANGED notification when
  // its container scrolls.
  // TODO(dmazzoni): remove once http://crbug.com/113483 is fixed.
  void TrackScrollingObject(BrowserAccessibilityWin* node);

  // Return a pointer to the object corresponding to the given windows-specific
  // unique id, does not make a new reference.
  BrowserAccessibilityWin* GetFromUniqueIdWin(LONG unique_id_win);

  // Called when |accessible_hwnd_| is deleted by its parent.
  void OnAccessibleHwndDeleted();

 protected:
  // BrowserAccessibilityManager methods
  virtual void OnRootChanged() OVERRIDE;

 private:
  // The closest ancestor HWND.
  HWND parent_hwnd_;

  // The accessibility instance for the parent window.
  IAccessible* parent_iaccessible_;

  // Give BrowserAccessibilityManager::Create access to our constructor.
  friend class BrowserAccessibilityManager;

  // Track the most recent object that has been asked to scroll and
  // post a notification directly on it when it reaches its destination.
  // TODO(dmazzoni): remove once http://crbug.com/113483 is fixed.
  BrowserAccessibilityWin* tracked_scroll_object_;

  // A mapping from the Windows-specific unique IDs (unique within the
  // browser process) to renderer ids within this page.
  base::hash_map<long, int32> unique_id_to_renderer_id_map_;

  // Owned by its parent; OnAccessibleHwndDeleted gets called upon deletion.
  LegacyRenderWidgetHostHWND* accessible_hwnd_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManagerWin);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_
