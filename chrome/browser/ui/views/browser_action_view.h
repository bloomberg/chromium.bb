// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BROWSER_ACTION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BROWSER_ACTION_VIEW_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/image_loading_tracker.h"
#include "content/public/browser/notification_observer.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/view.h"

class BrowserActionsContainer;
class ExtensionAction;

namespace views {
class MenuItemView;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserActionButton

// The BrowserActionButton is a specialization of the MenuButton class.
// It acts on a ExtensionAction, in this case a BrowserAction and handles
// loading the image for the button asynchronously on the file thread.
class BrowserActionButton : public views::MenuButton,
                            public views::ButtonListener,
                            public ImageLoadingTracker::Observer,
                            public content::NotificationObserver {
 public:
  BrowserActionButton(const extensions::Extension* extension,
                      BrowserActionsContainer* panel);

  // Call this instead of delete.
  void Destroy();

  ExtensionAction* browser_action() const { return browser_action_; }
  const extensions::Extension* extension() { return extension_; }

  // Called to update the display to match the browser action's state.
  void UpdateState();

  // Returns the default icon, if any.
  const SkBitmap& default_icon() const { return default_icon_; }

  // Does this button's action have a popup?
  virtual bool IsPopup();
  virtual GURL GetPopupUrl();

  // Overridden from views::View:
  virtual bool CanHandleAccelerators() const OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Overridden from ImageLoadingTracker.
  virtual void OnImageLoaded(const gfx::Image& image,
                             const std::string& extension_id,
                             int index) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // MenuButton behavior overrides.  These methods all default to TextButton
  // behavior unless this button is a popup.  In that case, it uses MenuButton
  // behavior.  MenuButton has the notion of a child popup being shown where the
  // button will stay in the pushed state until the "menu" (a popup in this
  // case) is dismissed.
  virtual bool Activate() OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyReleased(const views::KeyEvent& event) OVERRIDE;
  virtual void ShowContextMenu(const gfx::Point& p,
                               bool is_mouse_gesture) OVERRIDE;

  // Overridden from ui::AcceleratorTarget.
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

  // Notifications when to set button state to pushed/not pushed (for when the
  // popup/context menu is hidden or shown by the container).
  void SetButtonPushed();
  void SetButtonNotPushed();

 protected:
  // Overridden from views::View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    View* parent,
                                    View* child) OVERRIDE;

 private:
  virtual ~BrowserActionButton();

  // Register an extension command if the extension has an active one.
  void MaybeRegisterExtensionCommand();

  // Unregisters an extension command, if the extension has registered one and
  // it is active.
  void MaybeUnregisterExtensionCommand(bool only_if_active);

  // The browser action this view represents. The ExtensionAction is not owned
  // by this class.
  ExtensionAction* browser_action_;

  // The extension associated with the browser action we're displaying.
  const extensions::Extension* extension_;

  // The object that is waiting for the image loading to complete
  // asynchronously.
  ImageLoadingTracker tracker_;

  // The default icon for our browser action. This might be non-empty if the
  // browser action had a value for default_icon in the manifest.
  SkBitmap default_icon_;

  // The browser action shelf.
  BrowserActionsContainer* panel_;

  // The context menu.  This member is non-NULL only when the menu is shown.
  views::MenuItemView* context_menu_;

  content::NotificationRegistrar registrar_;

  // The extension keybinding accelerator this browser action is listening for
  // (to show the popup).
  scoped_ptr<ui::Accelerator> keybinding_;

  friend class base::DeleteHelper<BrowserActionButton>;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionButton);
};


////////////////////////////////////////////////////////////////////////////////
// BrowserActionView
// A single section in the browser action container. This contains the actual
// BrowserActionButton, as well as the logic to paint the badge.

class BrowserActionView : public views::View {
 public:
  BrowserActionView(const extensions::Extension* extension,
                    BrowserActionsContainer* panel);
  virtual ~BrowserActionView();

  BrowserActionButton* button() { return button_; }

  // Allocates a canvas object on the heap and draws into it the icon for the
  // view as well as the badge (if any). Caller is responsible for deleting the
  // returned object.
  gfx::Canvas* GetIconWithBadge();

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

 protected:
  // Overridden from views::View to paint the badge on top of children.
  virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE;

 private:
  // The container for this view.
  BrowserActionsContainer* panel_;

  // The button this view contains.
  BrowserActionButton* button_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BROWSER_ACTION_VIEW_H_
