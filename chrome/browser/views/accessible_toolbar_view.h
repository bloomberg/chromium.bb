// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_ACCESSIBLE_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_VIEWS_ACCESSIBLE_TOOLBAR_VIEW_H_
#pragma once

#include "base/hash_tables.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "views/focus/focus_manager.h"
#include "views/view.h"

namespace views {
class FocusSearch;
}

// This class provides keyboard access to any view that extends it, typically
// a toolbar.  The user sets focus to a control in this view by pressing
// F6 to traverse all panes, or by pressing a shortcut that jumps directly
// to this toolbar.
class AccessibleToolbarView : public views::View,
                              public views::FocusChangeListener,
                              public views::FocusTraversable {
 public:
  AccessibleToolbarView();
  virtual ~AccessibleToolbarView();

  // Set focus to the toolbar with complete keyboard access.
  // Focus will be restored to the ViewStorage with id |view_storage_id|
  // if the user escapes. If |initial_focus| is not NULL, that control will get
  // the initial focus, if it's enabled and focusable. Returns true if
  // the toolbar was able to receive focus.
  virtual bool SetToolbarFocus(int view_storage_id, View* initial_focus);

  // Set focus to the toolbar with complete keyboard access, with the
  // focus initially set to the default child. Focus will be restored
  // to the ViewStorage with id |view_storage_id| if the user escapes.
  // Returns true if the toolbar was able to receive focus.
  virtual bool SetToolbarFocusAndFocusDefault(int view_storage_id);

  // Overridden from views::View:
  virtual FocusTraversable* GetPaneFocusTraversable();
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);
  virtual void SetVisible(bool flag);
  virtual AccessibilityTypes::Role GetAccessibleRole();

  // Overridden from views::FocusChangeListener:
  virtual void FocusWillChange(View* focused_before,
                               View* focused_now);

  // Overridden from views::FocusTraversable:
  virtual views::FocusSearch* GetFocusSearch();
  virtual FocusTraversable* GetFocusTraversableParent();
  virtual View* GetFocusTraversableParentView();

 protected:
  // A subclass can override this to provide a default focusable child
  // other than the first focusable child.
  virtual views::View* GetDefaultFocusableChild() { return NULL; }

  // Remove toolbar focus.
  virtual void RemoveToolbarFocus();

  void RestoreLastFocusedView();

  View* GetFirstFocusableChild();
  View* GetLastFocusableChild();

  bool toolbar_has_focus_;

  ScopedRunnableMethodFactory<AccessibleToolbarView> method_factory_;

  // Save the focus manager rather than calling GetFocusManager(),
  // so that we can remove focus listeners in the destructor.
  views::FocusManager* focus_manager_;

  // Our custom focus search implementation that traps focus in this
  // toolbar and traverses all views that are focusable for accessibility,
  // not just those that are normally focusable.
  scoped_ptr<views::FocusSearch> focus_search_;

  // Registered accelerators
  views::Accelerator home_key_;
  views::Accelerator end_key_;
  views::Accelerator escape_key_;
  views::Accelerator left_key_;
  views::Accelerator right_key_;

  // Last focused view that issued this traversal.
  int last_focused_view_storage_id_;

  DISALLOW_COPY_AND_ASSIGN(AccessibleToolbarView);
};

#endif  // CHROME_BROWSER_VIEWS_ACCESSIBLE_TOOLBAR_VIEW_H_
