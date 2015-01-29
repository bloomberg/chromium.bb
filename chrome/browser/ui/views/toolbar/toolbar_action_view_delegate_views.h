// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_DELEGATE_VIEWS_H_

#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"

namespace views {
class FocusManager;
class MenuButton;
class View;
class Widget;
}

// The views-specific methods necessary for a ToolbarActionViewDelegate.
class ToolbarActionViewDelegateViews : public ToolbarActionViewDelegate {
 public:
  // Returns |this| as a view. We need this because our subclasses implement
  // different kinds of views, and inheriting View here is a really bad idea.
  virtual views::View* GetAsView() = 0;

  // Returns true if this view is being shown inside a menu.
  virtual bool IsShownInMenu() = 0;

  // Returns the FocusManager to use when registering accelerators.
  virtual views::FocusManager* GetFocusManagerForAccelerator() = 0;

  // Returns the parent for the associated context menu.
  virtual views::Widget* GetParentForContextMenu() = 0;

  // Returns the reference view for the extension action's popup.
  virtual views::View* GetReferenceViewForPopup() = 0;

  // Returns the MenuButton (if any) to use in showing the context menu (this
  // allows views code to update the pressed state of the button).
  virtual views::MenuButton* GetContextMenuButton() = 0;

 protected:
  ~ToolbarActionViewDelegateViews() override {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_DELEGATE_VIEWS_H_
