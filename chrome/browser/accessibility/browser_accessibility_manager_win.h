// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_
#define CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_
#pragma once

#include <atlbase.h>
#include <atlcom.h>
#include <oleacc.h>

#include <vector>

#include "chrome/browser/accessibility/browser_accessibility_manager.h"
#include "base/hash_tables.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_ptr.h"
#include "webkit/glue/webaccessibility.h"

class BrowserAccessibilityWin;
struct ViewHostMsg_AccessibilityNotification_Params;

class BrowserAccessibilityWinFactory {
 public:
  virtual ~BrowserAccessibilityWinFactory() {}

  // Create an instance of BrowserAccessibilityWin and return a new
  // reference to it.
  virtual BrowserAccessibilityWin* Create();
};

// Manages a tree of BrowserAccessibilityWin objects.
class BrowserAccessibilityManagerWin : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerWin(
      HWND parent_window,
      const webkit_glue::WebAccessibility& src,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityWinFactory* factory =
          new BrowserAccessibilityWinFactory());

  virtual ~BrowserAccessibilityManagerWin();

  // Return a pointer to the root of the tree, does not make a new reference.
  BrowserAccessibilityWin* GetRoot();

  // Removes the BrowserAccessibilityWin child_id from the manager.
  void Remove(LONG child_id);

  // Return a pointer to the object corresponding to the given child_id,
  // does not make a new reference.
  BrowserAccessibilityWin* GetFromChildID(LONG child_id);

  // Get a the default IAccessible for the parent window, does not make a
  // new reference.
  IAccessible* GetParentWindowIAccessible();

  // Return the object that has focus, if it's a descandant of the
  // given root (inclusive). Does not make a new reference.
  BrowserAccessibilityWin* GetFocus(BrowserAccessibilityWin* root);

  // Tell the renderer to set focus to this node.
  void SetFocus(const BrowserAccessibilityWin& node);

  // Tell the renderer to do the default action for this node.
  void DoDefaultAction(const BrowserAccessibilityWin& node);

  // BrowserAccessibilityManager Methods
  virtual IAccessible* GetRootAccessible();
  virtual void OnAccessibilityObjectStateChange(
      const webkit_glue::WebAccessibility& acc_obj);
  virtual void OnAccessibilityObjectChildrenChange(
      const webkit_glue::WebAccessibility& acc_obj);
  virtual void OnAccessibilityObjectFocusChange(
      const webkit_glue::WebAccessibility& acc_obj);
  virtual void OnAccessibilityObjectLoadComplete(
      const webkit_glue::WebAccessibility& acc_obj);
  virtual void OnAccessibilityObjectValueChange(
      const webkit_glue::WebAccessibility& acc_obj);
  virtual void OnAccessibilityObjectTextChange(
      const webkit_glue::WebAccessibility& acc_obj);

 private:
  // Recursively compare the IDs of our subtree to a new subtree received
  // from the renderer and return true if their IDs match exactly.
  bool CanModifyTreeInPlace(
      BrowserAccessibilityWin* current_root,
      const webkit_glue::WebAccessibility& new_root);

  // Recursively modify a subtree (by reinitializing) to match a new
  // subtree received from the renderer process. Should only be called
  // if CanModifyTreeInPlace returned true.
  void ModifyTreeInPlace(
      BrowserAccessibilityWin* current_root,
      const webkit_glue::WebAccessibility& new_root);

  // Update the accessibility tree with an updated WebAccessibility tree or
  // subtree received from the renderer process. First attempts to modify
  // the tree in place, and if that fails, replaces the entire subtree.
  // Returns the updated node or NULL if no node was updated.
  BrowserAccessibilityWin* UpdateTree(
      const webkit_glue::WebAccessibility& acc_obj);

  // Returns the next MSAA child id.
  static LONG GetNextChildID();

  // Recursively build a tree of BrowserAccessibilityWin objects from
  // the WebAccessibility tree received from the renderer process.
  BrowserAccessibilityWin* CreateAccessibilityTree(
      BrowserAccessibilityWin* parent,
      int child_id,
      const webkit_glue::WebAccessibility& src,
      int index_in_parent);

  // The object that can perform actions on our behalf.
  BrowserAccessibilityDelegate* delegate_;

  // Factory to create BrowserAccessibility objects (for dependency injection).
  scoped_ptr<BrowserAccessibilityWinFactory> factory_;

  // A default IAccessible instance for the parent window.
  ScopedComPtr<IAccessible> window_iaccessible_;

  // The root of the tree of IAccessible objects and the element that
  // currently has focus, if any.
  BrowserAccessibilityWin* root_;
  BrowserAccessibilityWin* focus_;

  // A mapping from the IDs of objects in the renderer, to the child IDs
  // we use internally here.
  base::hash_map<int, LONG> renderer_id_to_child_id_map_;

  // A mapping from child IDs to BrowserAccessibilityWin objects.
  base::hash_map<LONG, BrowserAccessibilityWin*> child_id_map_;

  // The next child ID to use; static so that they're global to the process.
  // Screen readers cache these IDs to see if they've seen the same object
  // before so we should avoid reusing them within the same project.
  static LONG next_child_id_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManagerWin);
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_
