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

class ExtensionAction;
class Profile;

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

    // Returns the primary ToolbarActionView associated with the given
    // |extension|.
    virtual ToolbarActionView* GetMainViewForAction(
        ToolbarActionView* view) = 0;

   protected:
    ~Delegate() override {}
  };

  ToolbarActionView(ToolbarActionViewController* view_controller,
                    Profile* profile,
                    Delegate* delegate);
  ~ToolbarActionView() override;

  // Modifies the given |border| in order to display a "popped out" for when
  // an action wants to run.
  static void DecorateWantsToRunBorder(views::LabelButtonBorder* border);

  // views::MenuButton:
  void GetAccessibleState(ui::AXViewState* state) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // MenuButton behavior overrides.  These methods all default to LabelButton
  // behavior unless this button is a popup.  In that case, it uses MenuButton
  // behavior.  MenuButton has the notion of a child popup being shown where the
  // button will stay in the pushed state until the "menu" (a popup in this
  // case) is dismissed.
  // TODO(devlin): This is a good idea, but it has some funny UI side-effects,
  // like the fact that label buttons enter a pressed state immediately, but
  // menu buttons only enter a pressed state on release (if they're draggable).
  // We should probably just pick a behavior, and stick to it.
  bool Activate() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  scoped_ptr<views::LabelButtonBorder> CreateDefaultBorder() const override;
  bool ShouldEnterPushedState(const ui::Event& event) override;

  // ToolbarActionViewDelegate: (public because called by others).
  void UpdateState() override;
  content::WebContents* GetCurrentWebContents() const override;

  ToolbarActionViewController* view_controller() {
    return view_controller_;
  }

  // Returns button icon so it can be accessed during tests.
  gfx::ImageSkia GetIconForTest();

  bool wants_to_run_for_testing() const { return wants_to_run_; }

 private:
  // views::MenuButton:
  gfx::Size GetPreferredSize() const override;
  const char* GetClassName() const override;
  void OnDragDone() override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void PaintChildren(gfx::Canvas* canvas,
                     const views::CullSet& cull_set) override;
  void OnPaintBorder(gfx::Canvas* canvas) override;

  // ToolbarActionViewDelegateViews:
  views::View* GetAsView() override;
  bool IsShownInMenu() override;
  views::FocusManager* GetFocusManagerForAccelerator() override;
  views::Widget* GetParentForContextMenu() override;
  ToolbarActionViewController* GetPreferredPopupViewController() override;
  views::View* GetReferenceViewForPopup() override;
  views::MenuButton* GetContextMenuButton() override;
  void OnPopupShown(bool by_user) override;
  void OnPopupClosed() override;

  // A lock to keep the MenuButton pressed when a menu or popup is visible.
  scoped_ptr<views::MenuButton::PressedLock> pressed_lock_;

  // The controller for this toolbar action view.
  ToolbarActionViewController* view_controller_;

  // The associated profile.
  Profile* profile_;

  // Delegate that usually represents a container for ToolbarActionView.
  Delegate* delegate_;

  // Used to make sure we only register the command once.
  bool called_register_command_;

  // The cached value of whether or not the action wants to run on the current
  // tab.
  bool wants_to_run_;

  // A special border to draw when the action wants to run.
  scoped_ptr<views::LabelButtonBorder> wants_to_run_border_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_H_
