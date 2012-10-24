// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_

#include <vector>

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "content/common/accessibility_node_data.h"
#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"

struct AccessibilityHostMsg_NotificationParams;

namespace content {
class BrowserAccessibility;
#if defined(OS_WIN)
class BrowserAccessibilityManagerWin;
#endif

// Class that can perform actions on behalf of the BrowserAccessibilityManager.
class CONTENT_EXPORT BrowserAccessibilityDelegate {
 public:
  virtual ~BrowserAccessibilityDelegate() {}
  virtual void SetAccessibilityFocus(int acc_obj_id) = 0;
  virtual void AccessibilityDoDefaultAction(int acc_obj_id) = 0;
  virtual void AccessibilityScrollToMakeVisible(
      int acc_obj_id, gfx::Rect subfocus) = 0;
  virtual void AccessibilityScrollToPoint(
      int acc_obj_id, gfx::Point point) = 0;
  virtual void AccessibilitySetTextSelection(
      int acc_obj_id, int start_offset, int end_offset) = 0;
  virtual bool HasFocus() const = 0;
  virtual gfx::Rect GetViewBounds() const = 0;
  virtual gfx::Point GetLastTouchEventLocation() const = 0;
};

class CONTENT_EXPORT BrowserAccessibilityFactory {
 public:
  virtual ~BrowserAccessibilityFactory() {}

  // Create an instance of BrowserAccessibility and return a new
  // reference to it.
  virtual BrowserAccessibility* Create();
};

// Manages a tree of BrowserAccessibility objects.
class CONTENT_EXPORT BrowserAccessibilityManager {
 public:
  // Creates the platform specific BrowserAccessibilityManager. Ownership passes
  // to the caller.
  static BrowserAccessibilityManager* Create(
    gfx::NativeView parent_view,
    const AccessibilityNodeData& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory = new BrowserAccessibilityFactory());

  // Creates the platform specific BrowserAccessibilityManager. Ownership passes
  // to the caller.
  static BrowserAccessibilityManager* CreateEmptyDocument(
    gfx::NativeView parent_view,
    AccessibilityNodeData::State state,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory = new BrowserAccessibilityFactory());

  virtual ~BrowserAccessibilityManager();

  // Type is enum AccessibilityNotification.
  // We pass it as int so that we don't include the message declaration
  // header here.
  virtual void NotifyAccessibilityEvent(
      int type,
      BrowserAccessibility* node) { }

  // Returns the next unique child id.
  static int32 GetNextChildID();

  // Return a pointer to the root of the tree, does not make a new reference.
  BrowserAccessibility* GetRoot();

  // Removes the BrowserAccessibility child_id and renderer_id from the manager.
  void Remove(int32 child_id, int32 renderer_id);

  // Return a pointer to the object corresponding to the given child_id,
  // does not make a new reference.
  BrowserAccessibility* GetFromChildID(int32 child_id);

  // Return a pointer to the object corresponding to the given renderer_id,
  // does not make a new reference.
  BrowserAccessibility* GetFromRendererID(int32 renderer_id);

  // Called to notify the accessibility manager that its associated native
  // view got focused. This implies that it is shown (opposite of WasHidden,
  // below).
  // The touch_event_context parameter indicates that we were called in the
  // context of a touch event.
  void GotFocus(bool touch_event_context);

  // Called to notify the accessibility manager that its associated native
  // view was hidden. When it's no longer hidden, GotFocus will be called.
  void WasHidden();

  // Called to notify the accessibility manager that a mouse down event
  // occurred in the tab.
  void GotMouseDown();

  // Update the focused node to |node|, which may be null.
  // If |notify| is true, send a message to the renderer to set focus
  // to this node.
  void SetFocus(BrowserAccessibility* node, bool notify);

  // Tell the renderer to do the default action for this node.
  void DoDefaultAction(const BrowserAccessibility& node);

  // Tell the renderer to scroll to make |node| visible.
  // In addition, if it's not possible to make the entire object visible,
  // scroll so that the |subfocus| rect is visible at least. The subfocus
  // rect is in local coordinates of the object itself.
  void ScrollToMakeVisible(
      const BrowserAccessibility& node, gfx::Rect subfocus);

  // Tell the renderer to scroll such that |node| is at |point|,
  // where |point| is in global coordinates of the WebContents.
  void ScrollToPoint(
      const BrowserAccessibility& node, gfx::Point point);

  // Tell the renderer to set the text selection on a node.
  void SetTextSelection(
      const BrowserAccessibility& node, int start_offset, int end_offset);

  // Retrieve the bounds of the parent View in screen coordinates.
  gfx::Rect GetViewBounds();

  // Called when the renderer process has notified us of about tree changes.
  // Send a notification to MSAA clients of the change.
  void OnAccessibilityNotifications(
      const std::vector<AccessibilityHostMsg_NotificationParams>& params);

  gfx::NativeView GetParentView();

#if defined(OS_WIN)
  BrowserAccessibilityManagerWin* ToBrowserAccessibilityManagerWin();
#endif

  // Return the object that has focus, if it's a descandant of the
  // given root (inclusive). Does not make a new reference.
  BrowserAccessibility* GetFocus(BrowserAccessibility* root);

  // Is the on-screen keyboard allowed to be shown, in response to a
  // focus event on a text box?
  bool IsOSKAllowed(const gfx::Rect& bounds);

 protected:
  BrowserAccessibilityManager(
      gfx::NativeView parent_view,
      const AccessibilityNodeData& src,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory);

 private:
  // The following states keep track of whether or not the
  // on-screen keyboard is allowed to be shown.
  enum OnScreenKeyboardState {
    // Never show the on-screen keyboard because this tab is hidden.
    OSK_DISALLOWED_BECAUSE_TAB_HIDDEN,

    // This tab was just shown, so don't pop-up the on-screen keyboard if a
    // text field gets focus that wasn't the result of an explicit touch.
    OSK_DISALLOWED_BECAUSE_TAB_JUST_APPEARED,

    // A touch event has occurred within the window, but focus has not
    // explicitly changed. Allow the on-screen keyboard to be shown if the
    // touch event was within the bounds of the currently focused object.
    // Otherwise we'll just wait to see if focus changes.
    OSK_ALLOWED_WITHIN_FOCUSED_OBJECT,

    // Focus has changed within a tab that's already visible. Allow the
    // on-screen keyboard to show anytime that a touch event leads to an
    // editable text control getting focus.
    OSK_ALLOWED
  };

  // Update an accessibility node with an updated AccessibilityNodeData node
  // received from the renderer process. When |include_children| is true
  // the node's children will also be updated, otherwise only the node
  // itself is updated.
  void UpdateNode(const AccessibilityNodeData& src, bool include_children);

  // Recursively build a tree of BrowserAccessibility objects from
  // the AccessibilityNodeData tree received from the renderer process.
  BrowserAccessibility* CreateAccessibilityTree(
      BrowserAccessibility* parent,
      const AccessibilityNodeData& src,
      int index_in_parent,
      bool send_show_events);

 protected:
  // The next unique id for a BrowserAccessibility instance.
  static int32 next_child_id_;

  // The parent view.
  gfx::NativeView parent_view_;

  // The object that can perform actions on our behalf.
  BrowserAccessibilityDelegate* delegate_;

  // Factory to create BrowserAccessibility objects (for dependency injection).
  scoped_ptr<BrowserAccessibilityFactory> factory_;

  // The root of the tree of IAccessible objects and the element that
  // currently has focus, if any.
  BrowserAccessibility* root_;
  BrowserAccessibility* focus_;

  // The on-screen keyboard state.
  OnScreenKeyboardState osk_state_;

  // A mapping from the IDs of objects in the renderer, to the child IDs
  // we use internally here.
  base::hash_map<int32, int32> renderer_id_to_child_id_map_;

  // A mapping from child IDs to BrowserAccessibility objects.
  base::hash_map<int32, BrowserAccessibility*> child_id_map_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_
