// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTION_VIEW_H_

#include "chrome/browser/extensions/extension_action_icon_factory.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/drag_controller.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

class Browser;
class BrowserActionButton;
class ExtensionAction;

namespace extensions {
class Extension;
class ExtensionToolbarModel;
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
  class Delegate : public views::DragController {
   public:
    // Returns the current tab's ID, or -1 if there is no current tab.
    virtual int GetCurrentTabId() const = 0;

    // Called when a browser action becomes visible/hidden.
    virtual void OnBrowserActionVisibilityChanged() = 0;

    // Whether the container for this button is shown inside a menu.
    virtual bool ShownInsideMenu() const = 0;

    // Notifies that a drag completed. Note this will only happen if the view
    // wasn't removed during the drag-and-drop process (i.e., not when there
    // was a move in the browser actions, since we re-create the views each
    // time we re-order the browser actions).
    virtual void OnBrowserActionViewDragDone() = 0;

    // Returns the view of the browser actions overflow menu to use as a
    // reference point for a popup when this view isn't visible.
    virtual views::View* GetOverflowReferenceView() = 0;

    // Sets the delegate's active popup owner to be |popup_owner|.
    virtual void SetPopupOwner(BrowserActionButton* popup_owner) = 0;

    // Hides the active popup of the delegate, if one exists.
    virtual void HideActivePopup() = 0;

    // Returns the Model backing the browser actions.
    virtual extensions::ExtensionToolbarModel* GetModel() = 0;

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
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

 protected:
  // Overridden from views::View to paint the badge on top of children.
  virtual void PaintChildren(gfx::Canvas* canvas,
                             const views::CullSet& cull_set) OVERRIDE;

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
                            public content::NotificationObserver,
                            public ExtensionActionIconFactory::Observer,
                            public views::WidgetObserver,
                            public ExtensionContextMenuModel::PopupDelegate {
 public:
  // The IconObserver will receive a notification when the button's icon has
  // been updated.
  class IconObserver {
   public:
    virtual void OnIconUpdated(const gfx::ImageSkia& icon) = 0;

   protected:
    virtual ~IconObserver() {}
  };

  BrowserActionButton(const extensions::Extension* extension,
                      Browser* browser_,
                      BrowserActionView::Delegate* delegate);

  // Call this instead of delete.
  void Destroy();

  ExtensionAction* browser_action() const { return browser_action_; }
  const extensions::Extension* extension() { return extension_; }

  void set_icon_observer(IconObserver* icon_observer) {
    icon_observer_ = icon_observer;
  }

  // Called to update the display to match the browser action's state.
  void UpdateState();

  // Does this button's action have a popup?
  virtual bool IsPopup();
  virtual GURL GetPopupUrl();

  // Show this extension's popup. If |grant_tab_permissions| is true, this will
  // grant the extension active tab permissions. Only do this if this was done
  // through a user action (and not e.g. an API).
  bool ShowPopup(ExtensionPopup::ShowAction show_action,
                 bool grant_tab_permissions);

  // Hides the popup, if one is open.
  void HidePopup();

  // Executes the browser action (and also shows the popup, if one exists).
  void ExecuteBrowserAction();

  // Overridden from views::View:
  virtual bool CanHandleAccelerators() const OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from views::ContextMenuController.
  virtual void ShowContextMenuForView(View* source,
                                      const gfx::Point& point,
                                      ui::MenuSourceType source_type) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overriden from ExtensionActionIconFactory::Observer.
  virtual void OnIconUpdated() OVERRIDE;

  // MenuButton behavior overrides.  These methods all default to LabelButton
  // behavior unless this button is a popup.  In that case, it uses MenuButton
  // behavior.  MenuButton has the notion of a child popup being shown where the
  // button will stay in the pushed state until the "menu" (a popup in this
  // case) is dismissed.
  virtual bool Activate() OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyReleased(const ui::KeyEvent& event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual scoped_ptr<views::LabelButtonBorder> CreateDefaultBorder() const
      OVERRIDE;

  // Overridden from ui::AcceleratorTarget.
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

  // ExtensionContextMenuModel::PopupDelegate:
  virtual void InspectPopup() OVERRIDE;

  // Notifications when to set button state to pushed/not pushed (for when the
  // popup/context menu is hidden or shown by the container).
  void SetButtonPushed();
  void SetButtonNotPushed();

  // Whether the browser action is enabled on this tab. Note that we cannot use
  // the built-in views enabled/SetEnabled because disabled views do not
  // receive drag events.
  bool IsEnabled(int tab_id) const;

  // Accessors.
  ExtensionActionIconFactory& icon_factory() { return icon_factory_; }
  ExtensionPopup* popup() { return popup_; }

  // Gets the icon of this button and its badge.
  gfx::ImageSkia GetIconWithBadge();

  // Returns button icon so it can be accessed during tests.
  gfx::ImageSkia GetIconForTest();

 protected:
  // Overridden from views::View:
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;
  virtual void OnDragDone() OVERRIDE;

 private:
  virtual ~BrowserActionButton();

  // views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // Register an extension command if the extension has an active one.
  void MaybeRegisterExtensionCommand();

  // Unregisters an extension command, if the extension has registered one and
  // it is active.
  void MaybeUnregisterExtensionCommand(bool only_if_active);

  // Cleans up after the popup. If |close_widget| is true, this will call
  // Widget::Close() on the popup's widget; otherwise it assumes the popup is
  // already closing.
  void CleanupPopup(bool close_widget);

  // The Browser object this button is associated with.
  Browser* browser_;

  // The browser action this view represents. The ExtensionAction is not owned
  // by this class.
  ExtensionAction* browser_action_;

  // The extension associated with the browser action we're displaying.
  const extensions::Extension* extension_;

  // The object that will be used to get the browser action icon for us.
  // It may load the icon asynchronously (in which case the initial icon
  // returned by the factory will be transparent), so we have to observe it for
  // updates to the icon.
  ExtensionActionIconFactory icon_factory_;

  // Delegate that usually represents a container for BrowserActionView.
  BrowserActionView::Delegate* delegate_;

  // Used to make sure MaybeRegisterExtensionCommand() is called only once
  // from ViewHierarchyChanged().
  bool called_registered_extension_command_;

  content::NotificationRegistrar registrar_;

  // The extension key binding accelerator this browser action is listening for
  // (to show the popup).
  scoped_ptr<ui::Accelerator> keybinding_;

  // Responsible for running the menu.
  scoped_ptr<views::MenuRunner> menu_runner_;

  // The browser action's popup, if it is visible; NULL otherwise.
  ExtensionPopup* popup_;

  // The observer that we need to notify when the icon of the button has been
  // updated.
  IconObserver* icon_observer_;

  friend class base::DeleteHelper<BrowserActionButton>;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTION_VIEW_H_
