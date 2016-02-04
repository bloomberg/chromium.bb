// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTIONS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTIONS_CONTAINER_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_delegate.h"
#include "chrome/browser/ui/views/extensions/extension_keybinding_registry_views.h"
#include "chrome/browser/ui/views/toolbar/chevron_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/controls/resize_area_delegate.h"
#include "ui/views/drag_controller.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

class ExtensionPopup;

namespace extensions {
class ActiveTabPermissionGranter;
class Command;
class Extension;
}

namespace views {
class BubbleDelegateView;
class ResizeArea;
}

// The BrowserActionsContainer is a container view, responsible for drawing the
// toolbar action icons (including extension icons and icons for component
// toolbar actions). It comes intwo flavors, a main container (when residing on
// the toolbar) and an overflow container (that resides in the main application
// menu, aka the Chrome menu).
//
// When in 'main' mode, the container supports the full functionality of a
// BrowserActionContainer, but in 'overflow' mode the container is effectively
// just an overflow for the 'main' toolbar (shows only the icons that the main
// toolbar does not) and as such does not have an overflow itself. The overflow
// container also does not support resizing. Since the main container only shows
// icons in the Chrome toolbar, it is limited to a single row of icons. The
// overflow container, however, is allowed to display icons in multiple rows.
//
// The main container is placed flush against the omnibox and hot dog menu,
// whereas the overflow container is placed within the hot dog menu. The
// layout is similar to this:
//   rI_I_IcCs
// Where the letters are as follows:
//   r: An invisible resize area.  This is
//      GetLayoutConstant(TOOLBAR_STANDARD_SPACING) pixels wide and directly
//      adjacent to the omnibox. Only shown for the main container.
//   I: An icon. In material design this has a width of 28. Otherwise it is as
//      wide as the IDR_BROWSER_ACTION image.
//   _: ToolbarActionsBar::PlatformSettings::item_spacing pixels of empty space.
//   c: GetChevronSpacing() pixels of empty space. Only present if C is present.
//   C: An optional chevron, as wide as the IDR_BROWSER_ACTIONS_OVERFLOW image,
//      and visible only when both of the following statements are true:
//      - The container is set to a width smaller than needed to show all icons.
//      - There is no other container in 'overflow' mode to handle the
//        non-visible icons for this container.
//   s: GetLayoutConstant(TOOLBAR_STANDARD_SPACING) pixels of empty space
//      (before the app menu).
// The reason the container contains the trailing space "s", rather than having
// it be handled by the parent view, is so that when the chevron is invisible
// and the user starts dragging an icon around, we have the space to draw the
// ultimate drop indicator.  (Otherwise, we'd be trying to draw it into the
// padding beyond our right edge, and it wouldn't appear.)
//
// The BrowserActionsContainer in 'main' mode follows a few rules, in terms of
// user experience:
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
// in the overflow. The install bubble for extensions points to the chevron
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
      public ToolbarActionsBarDelegate,
      public views::ResizeAreaDelegate,
      public gfx::AnimationDelegate,
      public ToolbarActionView::Delegate,
      public views::WidgetObserver,
      public extensions::ExtensionKeybindingRegistry::Delegate {
 public:
  // Constructs a BrowserActionContainer for a particular |browser| object. For
  // documentation of |main_container|, see class comments.
  BrowserActionsContainer(Browser* browser,
                          BrowserActionsContainer* main_container);
  ~BrowserActionsContainer() override;

  void Init();

  // Get the number of toolbar actions being displayed.
  size_t num_toolbar_actions() const { return toolbar_action_views_.size(); }

  // Returns the chevron, if any.
  views::View* chevron() { return chevron_; }
  const views::View* chevron() const { return chevron_; }

  // Returns the browser this container is associated with.
  Browser* browser() const { return browser_; }

  ToolbarActionsBar* toolbar_actions_bar() {
    return toolbar_actions_bar_.get();
  }

  // The class that registers for keyboard shortcuts for extension commands.
  extensions::ExtensionKeybindingRegistry* extension_keybinding_registry() {
    return extension_keybinding_registry_.get();
  }

  // Get a particular toolbar action view.
  ToolbarActionView* GetToolbarActionViewAt(int index) {
    return toolbar_action_views_[index];
  }

  // Whether we are performing resize animation on the container.
  bool animating() const {
    return resize_animation_ && resize_animation_->is_animating();
  }

  // Returns the ID of the action represented by the view at |index|.
  std::string GetIdAt(size_t index) const;

  // Returns the ToolbarActionView* associated with the given |extension|, or
  // NULL if none exists.
  ToolbarActionView* GetViewForId(const std::string& id);

  // Update the views to reflect the state of the toolbar actions.
  void RefreshToolbarActionViews();

  // Returns how many actions are currently visible. If the intent is to find
  // how many are visible once the container finishes animation, see
  // VisibleBrowserActionsAfterAnimation() below.
  size_t VisibleBrowserActions() const;

  // Returns how many actions will be visible once the container finishes
  // animating to a new size, or (if not animating) the currently visible icons.
  size_t VisibleBrowserActionsAfterAnimation() const;

  // Executes |command| registered by |extension|.
  void ExecuteExtensionCommand(const extensions::Extension* extension,
                               const extensions::Command& command);

  // Returns the preferred width given the limit of |max_width|. (Unlike most
  // views, since we don't want to show part of an icon or a large space after
  // the omnibox, this is probably *not* |max_width|).
  int GetWidthForMaxWidth(int max_width) const;

  // Overridden from views::View:
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  gfx::Size GetMinimumSize() const override;
  void Layout() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  bool GetDropFormats(
      int* formats,
      std::set<ui::Clipboard::FormatType>* format_types) override;
  bool AreDropTypesRequired() override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;
  void GetAccessibleState(ui::AXViewState* state) override;

  // Overridden from views::DragController:
  void WriteDragDataForView(View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;
  int GetDragOperationsForView(View* sender, const gfx::Point& p) override;
  bool CanStartDragForView(View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;

  // Overridden from views::ResizeAreaDelegate:
  void OnResize(int resize_amount, bool done_resizing) override;

  // Overridden from gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // Overridden from ToolbarActionView::Delegate:
  content::WebContents* GetCurrentWebContents() override;
  bool ShownInsideMenu() const override;
  void OnToolbarActionViewDragDone() override;
  views::MenuButton* GetOverflowReferenceView() override;
  void OnMouseEnteredToolbarActionView() override;

  // ToolbarActionsBarDelegate:
  void AddViewForAction(ToolbarActionViewController* action,
                        size_t index) override;
  void RemoveViewForAction(ToolbarActionViewController* action) override;
  void RemoveAllViews() override;
  void Redraw(bool order_changed) override;
  void ResizeAndAnimate(gfx::Tween::Type tween_type,
                        int target_width,
                        bool suppress_chevron) override;
  void SetChevronVisibility(bool chevron_visible) override;
  int GetWidth(GetWidthTime get_width_time) const override;
  bool IsAnimating() const override;
  void StopAnimating() override;
  int GetChevronWidth() const override;
  void ShowExtensionMessageBubble(
      scoped_ptr<extensions::ExtensionMessageBubbleController> controller,
      ToolbarActionViewController* anchor_action) override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // Overridden from extension::ExtensionKeybindingRegistry::Delegate:
  extensions::ActiveTabPermissionGranter* GetActiveTabPermissionGranter()
      override;

  views::BubbleDelegateView* active_bubble() { return active_bubble_; }

  ChevronMenuButton* chevron_for_testing() { return chevron_; }

 protected:
  // Overridden from views::View:
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

 private:
  // A struct representing the position at which an action will be dropped.
  struct DropPosition;

  typedef std::vector<ToolbarActionView*> ToolbarActionViews;

  void LoadImages();

  // Clears the |active_bubble_|, and unregisters the container as an observer.
  void ClearActiveBubble(views::Widget* widget);

  const ToolbarActionsBar::PlatformSettings& platform_settings() const {
    return toolbar_actions_bar_->platform_settings();
  }

  // Whether this container is in overflow mode (as opposed to in 'main'
  // mode). See class comments for details on the difference.
  bool in_overflow_mode() const { return main_container_ != NULL; }

  // The controlling ToolbarActionsBar, which handles most non-view logic.
  scoped_ptr<ToolbarActionsBar> toolbar_actions_bar_;

  // The vector of toolbar actions (icons/image buttons for each action).
  ToolbarActionViews toolbar_action_views_;

  // The Browser object the container is associated with.
  Browser* browser_;

  // The main container we are serving as overflow for, or NULL if this
  // class is the the main container. See class comments for details on
  // the difference between main and overflow.
  BrowserActionsContainer* main_container_;

  // The resize area for the container.
  views::ResizeArea* resize_area_;

  // The chevron for accessing the overflow items. Can be NULL when in overflow
  // mode or if the toolbar is permanently suppressing the chevron menu.
  ChevronMenuButton* chevron_;

  // The painter used when we are highlighting a subset of extensions.
  scoped_ptr<views::Painter> info_highlight_painter_;
  scoped_ptr<views::Painter> warning_highlight_painter_;

  // The animation that happens when the container snaps to place.
  scoped_ptr<gfx::SlideAnimation> resize_animation_;

  // Don't show the chevron while animating.
  bool suppress_chevron_;

  // True if the container has been added to the parent view.
  bool added_to_view_;

  // Whether or not the info bubble has been shown, if it should be.
  bool shown_bubble_;

  // When the container is resizing, this is the width at which it started.
  // If the container is not resizing, -1.
  int resize_starting_width_;

  // This is used while the user is resizing (and when the animations are in
  // progress) to know how wide the delta is between the current state and what
  // we should draw.
  int resize_amount_;

  // Keeps track of the absolute pixel width the container should have when we
  // are done animating.
  int animation_target_size_;

  // The DropPosition for the current drag-and-drop operation, or NULL if there
  // is none.
  scoped_ptr<DropPosition> drop_position_;

  // The class that registers for keyboard shortcuts for extension commands.
  scoped_ptr<ExtensionKeybindingRegistryViews> extension_keybinding_registry_;

  // The extension bubble that is actively showing, if any.
  views::BubbleDelegateView* active_bubble_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionsContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTIONS_CONTAINER_H_
