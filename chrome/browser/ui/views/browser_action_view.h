// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BROWSER_ACTION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BROWSER_ACTION_VIEW_H_

#include <string>

#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "content/public/browser/notification_observer.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/drag_controller.h"
#include "ui/views/view.h"

class Browser;
class BrowserActionButton;
class ExtensionAction;

namespace extensions {
class Extension;
}

namespace gfx {
class Image;
}

namespace views {
class MenuItemView;
class MenuRunner;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserActionView
// A single entry in the browser action container. This contains the actual
// BrowserActionButton, as well as the logic to paint the badge.
class BrowserActionView : public views::View {
 public:
  // Need DragController here because BrowserActionView could be
  // dragged/dropped.
  class Delegate : public views::DragController,
                   public ExtensionContextMenuModel::PopupDelegate {
   public:
    // Returns the current tab's ID, or -1 if there is no current tab.
    virtual int GetCurrentTabId() const = 0;

    // Called when the user clicks on the browser action icon.
    virtual void OnBrowserActionExecuted(BrowserActionButton* button) = 0;

    // Called when a browser action becomes visible/hidden.
    virtual void OnBrowserActionVisibilityChanged() = 0;

    // Returns relative position of a button inside BrowserActionView.
    virtual gfx::Point GetViewContentOffset() const = 0;

    virtual bool NeedToShowMultipleIconStates() const;
    virtual bool NeedToShowTooltip() const;

   protected:
    virtual ~Delegate() {}
  };

  BrowserActionView(const extensions::Extension* extension,
                    Browser* browser,
                    Delegate* delegate);
  virtual ~BrowserActionView();

  BrowserActionButton* button() { return button_; }

  // Gets browser action button icon with the badge.
  gfx::ImageSkia GetIconWithBadge();

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

 protected:
  // Overridden from views::View to paint the badge on top of children.
  virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE;

 private:
  // The Browser object this view is associated with.
  Browser* browser_;

  // Usually a container for this view.
  Delegate* delegate_;

  // The button this view contains.
  BrowserActionButton* button_;

  // Extension this view associated with.
  const extensions::Extension* extension_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionView);
};

////////////////////////////////////////////////////////////////////////////////
// BrowserActionButton

// The BrowserActionButton is a specialization of the MenuButton class.
// It acts on a ExtensionAction, in this case a BrowserAction and handles
// loading the image for the button asynchronously on the file thread.
class BrowserActionButton : public views::MenuButton,
                            public views::ButtonListener,
                            public views::ContextMenuController,
                            public ImageLoadingTracker::Observer,
                            public content::NotificationObserver {
 public:
  BrowserActionButton(const extensions::Extension* extension,
                      Browser* browser_,
                      BrowserActionView::Delegate* delegate);

  // Call this instead of delete.
  void Destroy();

  ExtensionAction* browser_action() const { return browser_action_; }
  const extensions::Extension* extension() { return extension_; }

  // Called to update the display to match the browser action's state.
  void UpdateState();

  // Does this button's action have a popup?
  virtual bool IsPopup();
  virtual GURL GetPopupUrl();

  // Overridden from views::View:
  virtual bool CanHandleAccelerators() const OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from views::ContextMenuController.
  virtual void ShowContextMenuForView(View* source,
                                      const gfx::Point& point) OVERRIDE;

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
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyReleased(const ui::KeyEvent& event) OVERRIDE;

  // Overridden from ui::AcceleratorTarget.
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

  // Notifications when to set button state to pushed/not pushed (for when the
  // popup/context menu is hidden or shown by the container).
  void SetButtonPushed();
  void SetButtonNotPushed();

  // Whether the browser action is enabled on this tab. Note that we cannot use
  // the built-in views enabled/SetEnabled because disabled views do not
  // receive drag events.
  bool IsEnabled(int tab_id) const;

  // Returns button icon so it can be accessed during tests.
  gfx::ImageSkia GetIconForTest();

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

  // The Browser object this button is associated with.
  Browser* browser_;

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

  // Delegate that usually represents a container for BrowserActionView.
  BrowserActionView::Delegate* delegate_;

  // The context menu.  This member is non-NULL only when the menu is shown.
  views::MenuItemView* context_menu_;

  // Used to make sure MaybeRegisterExtensionCommand() is called only once
  // from ViewHierarchyChanged().
  bool called_registered_extension_command_;

  content::NotificationRegistrar registrar_;

  // The extension key binding accelerator this browser action is listening for
  // (to show the popup).
  scoped_ptr<ui::Accelerator> keybinding_;

  // Responsible for running the menu.
  scoped_ptr<views::MenuRunner> menu_runner_;

  friend class base::DeleteHelper<BrowserActionButton>;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BROWSER_ACTION_VIEW_H_
