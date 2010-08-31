// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_
#define CHROME_BROWSER_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_
#pragma once

#include <atlbase.h>
#include <atlcom.h>
#include <oleacc.h>

#include <hash_map>
#include <vector>

#include "base/hash_tables.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_ptr.h"
#include "webkit/glue/webaccessibility.h"

class BrowserAccessibility;

class BrowserAccessibilityFactory {
 public:
  virtual ~BrowserAccessibilityFactory() {}

  // Create an instance of BrowserAccessibility and return a new
  // reference to it.
  virtual BrowserAccessibility* Create();
};

// Class that can perform actions on behalf of the BrowserAccessibilityManager.
class BrowserAccessibilityDelegate {
 public:
  virtual ~BrowserAccessibilityDelegate() {}
  virtual void SetAccessibilityFocus(int acc_obj_id) = 0;
  virtual void AccessibilityDoDefaultAction(int acc_obj_id) = 0;
  virtual void AccessibilityObjectChildrenChangeAck() = 0;
};

// Manages a tree of BrowserAccessibility objects.
class BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManager(
      HWND parent_hwnd,
      const webkit_glue::WebAccessibility& src,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory = new BrowserAccessibilityFactory());

  virtual ~BrowserAccessibilityManager();

  // Return a pointer to the root of the tree, does not make a new reference.
  BrowserAccessibility* GetRoot();

  // Removes the BrowserAccessibility child_id from the manager.
  void Remove(LONG child_id);

  // Return a pointer to the object corresponding to the given child_id,
  // does not make a new reference.
  BrowserAccessibility* GetFromChildID(LONG child_id);

  // Get a the default IAccessible for the parent window, does not make a
  // new reference.
  IAccessible* GetParentWindowIAccessible();

  // Get the parent window.
  HWND GetParentHWND();

  // Return the object that has focus, if it's a descandant of the
  // given root (inclusive). Does not make a new reference.
  BrowserAccessibility* GetFocus(BrowserAccessibility* root);

  // Tell the renderer to set focus to this node.
  void SetFocus(const BrowserAccessibility& node);

  // Tell the renderer to do the default action for this node.
  void DoDefaultAction(const BrowserAccessibility& node);

  // Called when the renderer process has notified us of a focus, state,
  // or children change. Send a notification to MSAA clients of the change.
  void OnAccessibilityFocusChange(int acc_obj_id);
  void OnAccessibilityObjectStateChange(int acc_obj_id);
  void OnAccessibilityObjectChildrenChange(
      const std::vector<webkit_glue::WebAccessibility>& acc_changes);

 private:
  // Returns the next MSAA child id.
  static LONG GetNextChildID();

  // Recursively build a tree of BrowserAccessibility objects from
  // the WebAccessibility tree received from the renderer process.
  BrowserAccessibility* CreateAccessibilityTree(
      BrowserAccessibility* parent,
      int child_id,
      const webkit_glue::WebAccessibility& src,
      int index_in_parent);

  // The parent window.
  HWND parent_hwnd_;

  // The object that can perform actions on our behalf.
  BrowserAccessibilityDelegate* delegate_;

  // Factory to create BrowserAccessibility objects (for dependency injection).
  scoped_ptr<BrowserAccessibilityFactory> factory_;

  // A default IAccessible instance for the parent window.
  ScopedComPtr<IAccessible> window_iaccessible_;

  // The root of the tree of IAccessible objects and the element that
  // currently has focus, if any.
  BrowserAccessibility* root_;
  BrowserAccessibility* focus_;

  // A mapping from the IDs of objects in the renderer, to the child IDs
  // we use internally here.
  base::hash_map<int, LONG> renderer_id_to_child_id_map_;

  // A mapping from child IDs to BrowserAccessibility objects.
  base::hash_map<LONG, BrowserAccessibility*> child_id_map_;

  // The next child ID to use; static so that they're global to the process.
  // Screen readers cache these IDs to see if they've seen the same object
  // before so we should avoid reusing them within the same project.
  static LONG next_child_id_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManager);
};

#endif  // CHROME_BROWSER_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_
