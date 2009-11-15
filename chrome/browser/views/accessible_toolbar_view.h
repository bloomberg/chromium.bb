// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_ACCESSIBLE_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_VIEWS_ACCESSIBLE_TOOLBAR_VIEW_H_

#include "views/view.h"

// This class provides keyboard access to any view that extends it by intiating
// ALT+SHIFT+T. And once you press TAB or SHIFT-TAB, it will traverse all the
// toolbars within Chrome. Child views are traversed in the order they were
// added.

class AccessibleToolbarView : public views::View {
 public:
  AccessibleToolbarView();
  virtual ~AccessibleToolbarView();

  // Initiate the traversal on the toolbar.
  void InitiateTraversal();

  // Overridden from views::View:
  virtual void DidGainFocus();
  virtual void WillLoseFocus();
  virtual bool OnKeyPressed(const views::KeyEvent& e);
  virtual bool OnKeyReleased(const views::KeyEvent& e);
  virtual bool SkipDefaultKeyEventProcessing(const views::KeyEvent& e);
  virtual void ShowContextMenu(int x, int y, bool is_mouse_gesture);
  virtual void RequestFocus();
  virtual bool GetAccessibleName(std::wstring* name);
  virtual bool GetAccessibleRole(AccessibilityTypes::Role* role);
  virtual void SetAccessibleName(const std::wstring& name);
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);
  virtual View* GetAccFocusedChildView() { return selected_focused_view_; }

 protected:
  // Returns the index of the next view of the toolbar, starting from the given
  // view index. |forward| when true means it will navigate from left to right,
  // and vice versa when false. If |view_index| is -1 the first accessible child
  // is returned.
  int GetNextAccessibleViewIndex(int view_index, bool forward);

  // Invoked from GetNextAccessibleViewIndex to determine if |view| can be
  // traversed to. Default implementation returns true, override to return false
  // for views you don't want reachable.
  virtual bool IsAccessibleViewTraversable(views::View* view);

 private:
  // Sets the focus to the currently |acc_focused_view_| view.
  void SetFocusToAccessibleView();

  // Storage of strings needed for accessibility.
  std::wstring accessible_name_;

  // Selected child view currently having accessibility focus.
  views::View* selected_focused_view_;

  // Last focused view that issued this traversal.
  int last_focused_view_storage_id_;

  DISALLOW_COPY_AND_ASSIGN(AccessibleToolbarView);
};

#endif  // CHROME_BROWSER_VIEWS_ACCESSIBLE_TOOLBAR_VIEW_H_
