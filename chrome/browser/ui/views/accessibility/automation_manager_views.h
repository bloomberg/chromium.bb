// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_AUTOMATION_MANAGER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_AUTOMATION_MANAGER_VIEWS_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/views/accessibility/ax_tree_source_views.h"

template <typename T> struct DefaultSingletonTraits;

class Profile;

namespace views {
class View;
}  // namespace views

// Manages a tree of automation nodes.
class AutomationManagerViews {
 public:
  // Get the single instance of this class.
  static AutomationManagerViews* GetInstance();

  // Handle an event fired upon a |View|.
  void HandleEvent(Profile* profile, views::View* view, ui::AXEvent event_type);

 private:
  friend struct DefaultSingletonTraits<AutomationManagerViews>;

  AutomationManagerViews();
  ~AutomationManagerViews();

  // Holds the active views-based accessibility tree. A tree currently consists
  // of all views descendant to a |Widget| (see |AXTreeSourceViews|).
  // A tree becomes active when an event is fired on a descendant view.
  scoped_ptr <views::AXTreeSourceViews> current_tree_;

  // Serializes incremental updates on the currently active tree
  // |current_tree_|.
  scoped_ptr<ui::AXTreeSerializer<views::View*> > current_tree_serializer_;

  DISALLOW_COPY_AND_ASSIGN(AutomationManagerViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_AUTOMATION_MANAGER_VIEWS_H_
