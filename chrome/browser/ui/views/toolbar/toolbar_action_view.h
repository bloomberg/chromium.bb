// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_H_

#include "chrome/browser/ui/views/toolbar/toolbar_action_view_delegate_views.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/drag_controller.h"
#include "ui/views/view.h"

class Browser;
class ExtensionAction;

namespace extensions {
class Extension;
}

namespace gfx {
class Image;
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarActionView
// A wrapper around a ToolbarActionViewController to display a toolbar action
// action in the BrowserActionsContainer.
class ToolbarActionView : public views::MenuButton,
                          public ToolbarActionViewDelegateViews,
                          public views::ButtonListener,
                          public content::NotificationObserver {
 public:
  // Need DragController here because ToolbarActionView could be
  // dragged/dropped.
  class Delegate : public views::DragController {
   public:
    // Returns the current web contents.
    virtual content::WebContents* GetCurrentWebContents() = 0;

    // Whether the container for this button is shown inside a menu.
    virtual bool ShownInsideMenu() const = 0;

    // Notifies that a drag completed.
    virtual void OnToolbarActionViewDragDone() = 0;

    // Returns the view of the toolbar actions overflow menu to use as a
    // reference point for a popup when this view isn't visible.
    virtual views::MenuButton* GetOverflowReferenceView() = 0;

    // Sets the delegate's active popup owner to be |popup_owner|.
    virtual void SetPopupOwner(ToolbarActionView* popup_owner) = 0;

    // Hides the active popup of the delegate, if one exists.
    virtual void HideActivePopup() = 0;

    // Returns the primary ToolbarActionView associated with the given
    // |extension|.
    virtual ToolbarActionView* GetMainViewForAction(
        ToolbarActionView* view) = 0;

   protected:
    ~Delegate() override {}
  };

  ToolbarActionView(scoped_ptr<ToolbarActionViewController> view_controller,
                    Browser* browser,
                    Delegate* delegate);
  ~ToolbarActionView() override;

  // Overridden from views::View:
  void GetAccessibleState(ui::AXViewState* state) override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // MenuButton behavior overrides.  These methods all default to LabelButton
  // behavior unless this button is a popup.  In that case, it uses MenuButton
  // behavior.  MenuButton has the notion of a child popup being shown where the
  // button will stay in the pushed state until the "menu" (a popup in this
  // case) is dismissed.
  bool Activate() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  scoped_ptr<views::LabelButtonBorder> CreateDefaultBorder() const override;

  // ToolbarActionViewDelegate: (public because called by others).
  void UpdateState() override;
  content::WebContents* GetCurrentWebContents() const override;

  ToolbarActionViewController* view_controller() {
    return view_controller_.get();
  }
  Browser* browser() { return browser_; }

  // Returns button icon so it can be accessed during tests.
  gfx::ImageSkia GetIconForTest();

 private:
  // Overridden from views::View:
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void OnDragDone() override;
  gfx::Size GetPreferredSize() const override;
  void PaintChildren(gfx::Canvas* canvas,
                     const views::CullSet& cull_set) override;

  // ToolbarActionViewDelegateViews:
  views::View* GetAsView() override;
  bool IsShownInMenu() override;
  views::FocusManager* GetFocusManagerForAccelerator() override;
  views::Widget* GetParentForContextMenu() override;
  ToolbarActionViewController* GetPreferredPopupViewController() override;
  views::View* GetReferenceViewForPopup() override;
  views::MenuButton* GetContextMenuButton() override;
  void HideActivePopup() override;
  void OnPopupShown(bool grant_tab_permissions) override;
  void CleanupPopup() override;

  // A lock to keep the MenuButton pressed when a menu or popup is visible.
  // This needs to be destroyed after |view_controller_|, because
  // |view_controller_|'s destructor can call CleanupPopup(), which uses this
  // object.
  scoped_ptr<views::MenuButton::PressedLock> pressed_lock_;

  // The controller for this toolbar action view.
  scoped_ptr<ToolbarActionViewController> view_controller_;

  // The associated browser.
  Browser* browser_;

  // Delegate that usually represents a container for ToolbarActionView.
  Delegate* delegate_;

  // Used to make sure we only register the command once.
  bool called_register_command_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_H_
