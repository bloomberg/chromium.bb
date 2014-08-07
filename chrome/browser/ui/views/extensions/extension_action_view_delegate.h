// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_VIEW_DELEGATE_H_

namespace content {
class WebContents;
}

namespace views {
class FocusManager;
class View;
class Widget;
}

// The view that surrounds an ExtensionAction and owns the
// ExtensionActionViewController. Since different actions can subclass
// different views, we don't derive views::View directly here.
class ExtensionActionViewDelegate {
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

  // Returns the current web contents.
  virtual content::WebContents* GetCurrentWebContents() = 0;

  // Hides whatever popup is active (even if it's not this one).
  virtual void HideActivePopup() = 0;

  // Called when the icon is updated; this is forwarded from the icon factory.
  virtual void OnIconUpdated() = 0;

  // Called when a popup is shown. See ExecuteAction() for the definition of
  // |grant_tab_permissions|.
  virtual void OnPopupShown(bool grant_tab_permissions) {}

  // Does any additional cleanup after the popup is closed.
  virtual void CleanupPopup() {}

  // Called immediately before the context menu is shown.
  virtual void OnWillShowContextMenus() {}

  // Called once the context menu has closed.
  // This may not be called if the context menu is showing and |this| is
  // deleted.
  virtual void OnContextMenuDone() {}

 protected:
  virtual ~ExtensionActionViewDelegate() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_VIEW_DELEGATE_H_
