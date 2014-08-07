// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTION_VIEW_H_

#include "chrome/browser/ui/views/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extension_action_view_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
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
    // Returns the current web contents.
    virtual content::WebContents* GetCurrentWebContents() = 0;

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

   protected:
    virtual ~Delegate() {}
  };

  BrowserActionView(const extensions::Extension* extension,
                    Browser* browser,
                    Delegate* delegate);
  virtual ~BrowserActionView();

  BrowserActionButton* button() { return button_.get(); }

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
  // Usually a container for this view.
  Delegate* delegate_;

  // The button this view contains.
  scoped_ptr<BrowserActionButton> button_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionView);
};

////////////////////////////////////////////////////////////////////////////////
// BrowserActionButton

// The BrowserActionButton is a specialization of the MenuButton class.
// This wraps an ExtensionActionView, and has knowledge of how to render itself
// and when to trigger the extension action.
class BrowserActionButton : public views::MenuButton,
                            public ExtensionActionViewDelegate,
                            public views::ButtonListener,
                            public content::NotificationObserver {
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
                      Browser* browser,
                      BrowserActionView::Delegate* delegate);
  virtual ~BrowserActionButton();

  const extensions::Extension* extension() const {
    return view_controller_->extension();
  }
  ExtensionAction* extension_action() {
    return view_controller_->extension_action();
  }
  ExtensionActionViewController* view_controller() {
    return view_controller_.get();
  }
  void set_icon_observer(IconObserver* icon_observer) {
    icon_observer_ = icon_observer;
  }

  // Called to update the display to match the browser action's state.
  void UpdateState();

  // Does this button's action have a popup?
  bool IsPopup();

  // Overridden from views::View:
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

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

  // Notifications when to set button state to pushed/not pushed (for when the
  // popup/context menu is hidden or shown by the container).
  void SetButtonPushed();
  void SetButtonNotPushed();

  // Whether the browser action is enabled on this tab. Note that we cannot use
  // the built-in views enabled/SetEnabled because disabled views do not
  // receive drag events.
  bool IsEnabled(int tab_id) const;

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
  // ExtensionActionViewDelegate:
  virtual views::View* GetAsView() OVERRIDE;
  virtual bool IsShownInMenu() OVERRIDE;
  virtual views::FocusManager* GetFocusManagerForAccelerator() OVERRIDE;
  virtual views::Widget* GetParentForContextMenu() OVERRIDE;
  virtual views::View* GetReferenceViewForPopup() OVERRIDE;
  virtual content::WebContents* GetCurrentWebContents() OVERRIDE;
  virtual void HideActivePopup() OVERRIDE;
  virtual void OnIconUpdated() OVERRIDE;
  virtual void OnPopupShown(bool grant_tab_permissions) OVERRIDE;
  virtual void CleanupPopup() OVERRIDE;
  virtual void OnWillShowContextMenus() OVERRIDE;
  virtual void OnContextMenuDone() OVERRIDE;

  // The controller for this ExtensionAction view.
  scoped_ptr<ExtensionActionViewController> view_controller_;

  // Delegate that usually represents a container for BrowserActionView.
  BrowserActionView::Delegate* delegate_;

  // Used to make sure we only register the command once.
  bool called_registered_extension_command_;

  content::NotificationRegistrar registrar_;

  // The observer that we need to notify when the icon of the button has been
  // updated.
  IconObserver* icon_observer_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTION_VIEW_H_
