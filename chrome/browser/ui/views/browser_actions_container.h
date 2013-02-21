// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BROWSER_ACTIONS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_BROWSER_ACTIONS_CONTAINER_H_

#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/ui/views/browser_action_view.h"
#include "chrome/browser/ui/views/chrome_views_export.h"
#include "chrome/browser/ui/views/extensions/browser_action_overflow_menu_controller.h"
#include "chrome/browser/ui/views/extensions/extension_keybinding_registry_views.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "content/public/browser/notification_observer.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/tween.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/resize_area_delegate.h"
#include "ui/views/drag_controller.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

class BrowserActionButton;
class ExtensionKeybindingRegistryViews;
class ExtensionPopup;

namespace extensions {
class ActiveTabPermissionGranter;
}

namespace ui {
class SlideAnimation;
}

namespace views {
class ResizeArea;
}

////////////////////////////////////////////////////////////////////////////////
//
// The BrowserActionsContainer is a container view, responsible for drawing the
// browser action icons (extensions that add icons to the toolbar).
//
// The container is placed flush against the omnibox and wrench menu, and its
// layout looks like:
//   rI_I_IcCs
// Where the letters are as follows:
//   r: An invisible resize area.  This is ToolbarView::kStandardSpacing pixels
//      wide and directly adjacent to the omnibox.
//   I: An icon.  This is as wide as the IDR_BROWSER_ACTION image.
//   _: kItemSpacing pixels of empty space.
//   c: kChevronSpacing pixels of empty space.  Only present if C is present.
//   C: An optional chevron, visible for overflow.  As wide as the
//      IDR_BROWSER_ACTIONS_OVERFLOW image.
//   s: ToolbarView::kStandardSpacing pixels of empty space (before the wrench
//      menu).
// The reason the container contains the trailing space "s", rather than having
// it be handled by the parent view, is so that when the chevron is invisible
// and the user starts dragging an icon around, we have the space to draw the
// ultimate drop indicator.  (Otherwise, we'd be trying to draw it into the
// padding beyond our right edge, and it wouldn't appear.)
//
// The BrowserActionsContainer follows a few rules, in terms of user experience:
//
// 1) The container can never grow beyond the space needed to show all icons
// (hereby referred to as the max width).
// 2) The container can never shrink below the space needed to show just the
// initial padding and the chevron (ignoring the case where there are no icons
// to show, in which case the container won't be visible anyway).
// 3) The container snaps into place (to the pixel count that fits the visible
// icons) to make sure there is no wasted space at the edges of the container.
// 4) If the user adds or removes icons (read: installs/uninstalls browser
// actions) we grow and shrink the container as needed - but ONLY if the
// container was at max width to begin with.
// 5) If the container is NOT at max width (has an overflow menu), we respect
// that size when adding and removing icons and DON'T grow/shrink the container.
// This means that new icons (which always appear at the far right) will show up
// in the overflow menu. The install bubble for extensions points to the chevron
// menu in this case.
//
// Resizing the BrowserActionsContainer:
//
// The ResizeArea view sends OnResize messages to the BrowserActionsContainer
// class as the user drags it. This modifies the value for |resize_amount_|.
// That indicates to the container that a resize is in progress and is used to
// calculate the size in GetPreferredSize(), though that function never exceeds
// the defined minimum and maximum size of the container.
//
// When the user releases the mouse (ends the resize), we calculate a target
// size for the container (animation_target_size_), clamp that value to the
// containers min and max and then animate from the *current* position (that the
// user has dragged the view to) to the target size.
//
// Animating the BrowserActionsContainer:
//
// Animations are used when snapping the container to a value that fits all
// visible icons. This can be triggered when the user finishes resizing the
// container or when Browser Actions are added/removed.
//
// We always animate from the current width (container_width_) to the target
// size (animation_target_size_), using |resize_amount| to keep track of the
// animation progress.
//
// NOTE: When adding Browser Actions to a maximum width container (no overflow)
// we make sure to suppress the chevron menu if it wasn't visible. This is
// because we won't have enough space to show the new Browser Action until the
// animation ends and we don't want the chevron to flash into view while we are
// growing the container.
//
////////////////////////////////////////////////////////////////////////////////
class BrowserActionsContainer
    : public views::View,
      public views::MenuButtonListener,
      public views::ResizeAreaDelegate,
      public ui::AnimationDelegate,
      public ExtensionToolbarModel::Observer,
      public BrowserActionOverflowMenuController::Observer,
      public views::WidgetObserver,
      public BrowserActionView::Delegate,
      public extensions::ExtensionKeybindingRegistry::Delegate {
 public:
  BrowserActionsContainer(Browser* browser, views::View* owner_view);
  virtual ~BrowserActionsContainer();

  void Init();

  // Get the number of browser actions being displayed.
  int num_browser_actions() const { return browser_action_views_.size(); }

  // Whether we are performing resize animation on the container.
  bool animating() const { return animation_target_size_ > 0; }

  // Returns the chevron, if any.
  views::View* chevron() { return chevron_; }
  const views::View* chevron() const { return chevron_; }

  // Returns the profile this container is associated with.
  Profile* profile() const { return profile_; }

  // Get a particular browser action view.
  BrowserActionView* GetBrowserActionViewAt(int index) {
    return browser_action_views_[index];
  }

  // Retrieve the BrowserActionView for |extension|.
  BrowserActionView* GetBrowserActionView(ExtensionAction* action);

  // Update the views to reflect the state of the browser action icons.
  void RefreshBrowserActionViews();

  // Sets up the browser action view vector.
  void CreateBrowserActionViews();

  // Delete all browser action views.
  void DeleteBrowserActionViews();

  // Returns how many browser actions are visible.
  size_t VisibleBrowserActions() const;

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual bool GetDropFormats(int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats) OVERRIDE;
  virtual bool AreDropTypesRequired() OVERRIDE;
  virtual bool CanDrop(const ui::OSExchangeData& data) OVERRIDE;
  virtual void OnDragEntered(const ui::DropTargetEvent& event) OVERRIDE;
  virtual int OnDragUpdated(const ui::DropTargetEvent& event) OVERRIDE;
  virtual void OnDragExited() OVERRIDE;
  virtual int OnPerformDrop(const ui::DropTargetEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // Overridden from views::MenuButtonListener:
  virtual void OnMenuButtonClicked(views::View* source,
                                   const gfx::Point& point) OVERRIDE;

  // Overridden from views::DragController:
  virtual void WriteDragDataForView(View* sender,
                                    const gfx::Point& press_pt,
                                    ui::OSExchangeData* data) OVERRIDE;
  virtual int GetDragOperationsForView(View* sender,
                                       const gfx::Point& p) OVERRIDE;
  virtual bool CanStartDragForView(View* sender,
                                   const gfx::Point& press_pt,
                                   const gfx::Point& p) OVERRIDE;

  // Overridden from views::ResizeAreaDelegate:
  virtual void OnResize(int resize_amount, bool done_resizing) OVERRIDE;

  // Overridden from ui::AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

  // Overridden from BrowserActionOverflowMenuController::Observer:
  virtual void NotifyMenuDeleted(
      BrowserActionOverflowMenuController* controller) OVERRIDE;

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // Overridden from BrowserActionView::Delegate:
  virtual void InspectPopup(ExtensionAction* action) OVERRIDE;
  virtual int GetCurrentTabId() const OVERRIDE;
  virtual void OnBrowserActionExecuted(BrowserActionButton* button) OVERRIDE;
  virtual void OnBrowserActionVisibilityChanged() OVERRIDE;
  virtual gfx::Point GetViewContentOffset() const OVERRIDE;

  // Overridden from extension::ExtensionKeybindingRegistry::Delegate:
  virtual extensions::ActiveTabPermissionGranter*
      GetActiveTabPermissionGranter() OVERRIDE;

  // Moves a browser action with |id| to |new_index|.
  void MoveBrowserAction(const std::string& extension_id, size_t new_index);

  // Hide the current popup.
  void HidePopup();

  // Simulate a click on a browser action button.  This should only be
  // used by unit tests.
  void TestExecuteBrowserAction(int index);

  // Retrieve the current popup.  This should only be used by unit tests.
  ExtensionPopup* TestGetPopup() { return popup_; }

  // Set how many icons the container should show. This should only be used by
  // unit tests.
  void TestSetIconVisibilityCount(size_t icons);

  // During testing we can disable animations by setting this flag to true,
  // so that the bar resizes instantly, instead of having to poll it while it
  // animates to open/closed status.
  static bool disable_animations_during_testing_;

 protected:
  // Overridden from views::View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnThemeChanged() OVERRIDE;

 private:
  friend class BrowserActionView;  // So it can access IconHeight().
  friend class ShowFolderMenuTask;

  typedef std::vector<BrowserActionView*> BrowserActionViews;

  // Returns the width of an icon, optionally with its padding.
  static int IconWidth(bool include_padding);

  // Returns the height of an icon.
  static int IconHeight();

  // ExtensionToolbarModel::Observer implementation.
  virtual void BrowserActionAdded(const extensions::Extension* extension,
                                  int index) OVERRIDE;
  virtual void BrowserActionRemoved(
      const extensions::Extension* extension) OVERRIDE;
  virtual void BrowserActionMoved(const extensions::Extension* extension,
                                  int index) OVERRIDE;
  virtual void ModelLoaded() OVERRIDE;

  void LoadImages();

  // Sets the initial container width.
  void SetContainerWidth();

  // Closes the overflow menu if open.
  void CloseOverflowMenu();

  // Cancels the timer for showing the drop down menu.
  void StopShowFolderDropMenuTimer();

  // Show the drop down folder after a slight delay.
  void StartShowFolderDropMenuTimer();

  // Show the overflow menu.
  void ShowDropFolder();

  // Sets the drop indicator position (and schedules paint if the position has
  // changed).
  void SetDropIndicator(int x_pos);

  // Given a number of |icons| and whether to |display_chevron|, returns the
  // amount of pixels needed to draw the entire container.  For convenience,
  // callers can set |icons| to -1 to mean "all icons".
  int IconCountToWidth(int icons, bool display_chevron) const;

  // Given a pixel width, returns the number of icons that fit.  (This
  // automatically determines whether a chevron will be needed and includes it
  // in the calculation.)
  size_t WidthToIconCount(int pixels) const;

  // Returns the absolute minimum size you can shrink the container down to and
  // still show it.  This assumes a visible chevron because the only way we
  // would not have a chevron when shrinking down this far is if there were no
  // icons, in which case the container wouldn't be shown at all.
  int ContainerMinSize() const;

  // Animate to the target size (unless testing, in which case we go straight to
  // the target size).  This also saves the target number of visible icons in
  // the pref if we're not incognito.
  void SaveDesiredSizeAndAnimate(ui::Tween::Type type,
                                 size_t num_visible_icons);

  // Returns true if this extension should be shown in this toolbar. This can
  // return false if we are in an incognito window and the extension is disabled
  // for incognito.
  bool ShouldDisplayBrowserAction(const extensions::Extension* extension);

  // Show a popup.
  void ShowPopup(BrowserActionButton* button,
                 ExtensionPopup::ShowAction show_action);

  // The vector of browser actions (icons/image buttons for each action). Note
  // that not every BrowserAction in the ToolbarModel will necessarily be in
  // this collection. Some extensions may be disabled in incognito windows.
  BrowserActionViews browser_action_views_;

  Profile* profile_;

  // The Browser object the container is associated with.
  Browser* browser_;

  // The view that owns us.
  views::View* owner_view_;

  // The current popup and the button it came from.  NULL if no popup.
  ExtensionPopup* popup_;

  // The button that triggered the current popup (just a reference to a button
  // from browser_action_views_).
  BrowserActionButton* popup_button_;

  // The model that tracks the order of the toolbar icons.
  ExtensionToolbarModel* model_;

  // The current width of the container.
  int container_width_;

  // The resize area for the container.
  views::ResizeArea* resize_area_;

  // The chevron for accessing the overflow items.
  views::MenuButton* chevron_;

  // The menu to show for the overflow button (chevron). This class manages its
  // own lifetime so that it can stay alive during drag and drop operations.
  BrowserActionOverflowMenuController* overflow_menu_;

  // The animation that happens when the container snaps to place.
  scoped_ptr<ui::SlideAnimation> resize_animation_;

  // Don't show the chevron while animating.
  bool suppress_chevron_;

  // This is used while the user is resizing (and when the animations are in
  // progress) to know how wide the delta is between the current state and what
  // we should draw.
  int resize_amount_;

  // Keeps track of the absolute pixel width the container should have when we
  // are done animating.
  int animation_target_size_;

  // The x position for where to draw the drop indicator. -1 if no indicator.
  int drop_indicator_position_;

  // The class that registers for keyboard shortcuts for extension commands.
  scoped_ptr<ExtensionKeybindingRegistryViews> extension_keybinding_registry_;

  base::WeakPtrFactory<BrowserActionsContainer> task_factory_;

  // Handles delayed showing of the overflow menu when hovering.
  base::WeakPtrFactory<BrowserActionsContainer> show_menu_task_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionsContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BROWSER_ACTIONS_CONTAINER_H_
