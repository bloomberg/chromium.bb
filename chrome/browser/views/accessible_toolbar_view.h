// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_ACCESSIBLE_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_VIEWS_ACCESSIBLE_TOOLBAR_VIEW_H_

#include "views/view.h"

// This class provides keyboard access to any view that extends it by intiating
// ALT+SHIFT+T. And once you press TAB or SHIFT-TAB, it will traverse all the
// toolbars within Chrome. It traverses child views in the order of adding them,
// not necessarily the visual order.
//
// If a toolbar needs to be traversal then it needs to be focusable. But we do
// not want the toolbar to have any focus, so some hacking needs to be done to
// tell the focus manager that this view is focusable while it's not. The main
// purpose of this class is to provide keyboard access to any View that extends
// this. It will properly hot track and add the tooltip to the first level
// children if visited.

class AccessibleToolbarView : public views::View {
 public:
  AccessibleToolbarView();
  virtual ~AccessibleToolbarView();

  // Returns the index of the next view of the toolbar, starting from the given
  // view index, in the given navigation direction, |forward| when true means it
  // will navigate from left to right, and vice versa when false. When
  // |view_index| is -1, it finds the first accessible child.
  int GetNextAccessibleViewIndex(int view_index, bool forward);

  // Invoked when you press left and right arrows while traversing in the view.
  // The default implementation is true, where every child in the view is
  // traversable.
  virtual bool IsAccessibleViewTraversable(views::View* view);

  // Sets the accessible focused view.
  void set_acc_focused_view(views::View* acc_focused_view) {
    acc_focused_view_ = acc_focused_view;
  }

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
  virtual View* GetAccFocusedChildView() { return acc_focused_view_; }

 private:
  // Sets the focus to the currently |acc_focused_view_| view.
  void SetFocusToAccessibleView();

  // Storage of strings needed for accessibility.
  std::wstring accessible_name_;

  // Child view currently having accessibility focus.
  views::View* acc_focused_view_;

  // Last focused view that issued this traversal.
  int last_focused_view_storage_id_;

  DISALLOW_COPY_AND_ASSIGN(AccessibleToolbarView);
};

#endif  // CHROME_BROWSER_VIEWS_ACCESSIBLE_TOOLBAR_VIEW_H_
