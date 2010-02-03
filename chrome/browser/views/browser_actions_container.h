// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BROWSER_ACTIONS_CONTAINER_H_
#define CHROME_BROWSER_VIEWS_BROWSER_ACTIONS_CONTAINER_H_

#include <vector>

#include "base/task.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/views/browser_bubble.h"
#include "chrome/browser/views/extensions/extension_action_context_menu.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/controls/resize_gripper.h"
#include "views/view.h"

class BrowserActionOverflowMenuController;
class BrowserActionsContainer;
class Extension;
class ExtensionAction;
class ExtensionPopup;
class PrefService;
class Profile;
class SlideAnimation;
class ToolbarView;

////////////////////////////////////////////////////////////////////////////////
// BrowserActionButton

// The BrowserActionButton is a specialization of the MenuButton class.
// It acts on a ExtensionAction, in this case a BrowserAction and handles
// loading the image for the button asynchronously on the file thread.
class BrowserActionButton : public views::MenuButton,
                            public views::ButtonListener,
                            public ImageLoadingTracker::Observer,
                            public NotificationObserver {
 public:
  BrowserActionButton(Extension* extension, BrowserActionsContainer* panel);
  ~BrowserActionButton();

  ExtensionAction* browser_action() const { return browser_action_; }
  Extension* extension() { return extension_; }

  // Called to update the display to match the browser action's state.
  void UpdateState();

  // Returns the default icon, if any.
  const SkBitmap& default_icon() const { return default_icon_; }

  // Overridden from views::View. Return a 0-inset so the icon can draw all the
  // way to the edge of the view if it wants.
  virtual gfx::Insets GetInsets() const;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from ImageLoadingTracker.
  virtual void OnImageLoaded(SkBitmap* image, size_t index);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // MenuButton behavior overrides.  These methods all default to TextButton
  // behavior unless this button is a popup.  In that case, it uses MenuButton
  // behavior.  MenuButton has the notion of a child popup being shown where the
  // button will stay in the pushed state until the "menu" (a popup in this
  // case) is dismissed.
  virtual bool Activate();
  virtual bool OnMousePressed(const views::MouseEvent& e);
  virtual void OnMouseReleased(const views::MouseEvent& e, bool canceled);
  virtual bool OnKeyReleased(const views::KeyEvent& e);
  virtual void OnMouseExited(const views::MouseEvent& event);

  // Does this button's action have a popup?
  virtual bool IsPopup();
  virtual GURL GetPopupUrl();

  // Notifications when to set button state to pushed/not pushed (for when the
  // popup/context menu is hidden or shown by the container).
  virtual void SetButtonPushed();
  virtual void SetButtonNotPushed();

 private:
  // The browser action this view represents. The ExtensionAction is not owned
  // by this class.
  ExtensionAction* browser_action_;

  // The extension associated with the browser action we're displaying.
  Extension* extension_;

  // The object that is waiting for the image loading to complete
  // asynchronously. This object can potentially outlive the BrowserActionView,
  // and takes care of deleting itself.
  ImageLoadingTracker* tracker_;

  // The context menu for browser action icons.
  scoped_ptr<ExtensionActionContextMenu> context_menu_;

  // Whether we are currently showing/just finished showing a context menu.
  bool showing_context_menu_;

  // The default icon for our browser action. This might be non-empty if the
  // browser action had a value for default_icon in the manifest.
  SkBitmap default_icon_;

  // The browser action shelf.
  BrowserActionsContainer* panel_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionButton);
};


////////////////////////////////////////////////////////////////////////////////
// BrowserActionView
// A single section in the browser action container. This contains the actual
// BrowserActionButton, as well as the logic to paint the badge.

class BrowserActionView : public views::View {
 public:
  BrowserActionView(Extension* extension, BrowserActionsContainer* panel);
  BrowserActionButton* button() { return button_; }

 private:
  virtual void Layout();

  // Override PaintChildren so that we can paint the badge on top of children.
  virtual void PaintChildren(gfx::Canvas* canvas);

  // The container for this view.
  BrowserActionsContainer* panel_;

  // The button this view contains.
  BrowserActionButton* button_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionView);
};

////////////////////////////////////////////////////////////////////////////////
//
// The BrowserActionsContainer is a container view, responsible for drawing the
// browser action icons (extensions that add icons to the toolbar).
//
// The BrowserActionsContainer (when it contains one or more icons) consists of
// the following elements, numbered as seen below the line:
//
//    || _ Icon _ Icon _ Icon _ [chevron] _ | _
//    -----------------------------------------
//     1 2   3  4                   5     6 7 8
//
// 1) The ResizeGripper view.
// 2) Padding  (kHorizontalPadding).
// 3) The browser action icon button (BrowserActionView).
// 4) Padding to visually separate icons from one another
//    (kBrowserActionButtonPadding). Not included if only one icon visible.
// 5) The chevron menu (MenuButton), shown when there is not enough room to show
//    all the icons.
// 6) Padding  (kDividerHorizontalMargin).
// 7) A thin vertical divider drawn during Paint to create visual separation for
//    the container from the Page and Wrench menus.
// 8) Padding  (kChevronRightMargin).
//
// The BrowserActionsContainer follows a few rules, in terms of user experience:
//
// 1) The container can never grow beyond the space needed to show all icons
// (hereby referred to as the max width).
// 2) The container can never shrink below the space needed to show just the
// resize gripper and the chevron (ignoring the case where there are no icons to
// show, in which case the container won't be visible anyway).
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
// The ResizeGripper view sends OnResize messages to the BrowserActionsContainer
// class as the user drags the gripper. This modifies the value for
// |resize_amount_|. That indicates to the container that a resize is in
// progress and is used to calculate the size in GetPreferredSize(), though
// that function never exceeds the defined minimum and maximum size of the
// container.
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
// We always animate from the current width (container_size_.width()) to the
// target size (animation_target_size_), using |resize_amount| to keep track of
// the animation progress.
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
    public NotificationObserver,
    public BrowserBubble::Delegate,
    public views::ViewMenuDelegate,
    public views::DragController,
    public views::ResizeGripper::ResizeGripperDelegate,
    public AnimationDelegate,
    public ExtensionToolbarModel::Observer {
 public:
  BrowserActionsContainer(Profile* profile, ToolbarView* toolbar);
  virtual ~BrowserActionsContainer();

  static void RegisterUserPrefs(PrefService* prefs);

  // Get the number of browser actions being displayed.
  int num_browser_actions() const { return browser_action_views_.size(); }

  // Whether we are performing resize animation on the container.
  bool animating() const { return animation_target_size_ > 0; }

  // Returns the chevron, if any.
  const views::View* chevron() const { return chevron_; }

  // Returns the current tab's ID, or -1 if there is no current tab.
  int GetCurrentTabId() const;

  // Get a particular browser action view.
  BrowserActionView* GetBrowserActionViewAt(int index) {
    return browser_action_views_[index];
  }

  // Retrieve the BrowserActionView for |extension|.
  BrowserActionView* GetBrowserActionView(Extension* extension);

  // Update the views to reflect the state of the browser action icons.
  void RefreshBrowserActionViews();

  // Sets up the browser action view vector.
  void CreateBrowserActionViews();

  // Delete all browser action views.
  void DeleteBrowserActionViews();

  // Called when a browser action becomes visible/hidden.
  void OnBrowserActionVisibilityChanged();

  // Called when the user clicks on the browser action icon.
  void OnBrowserActionExecuted(BrowserActionButton* button);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void Paint(gfx::Canvas* canvas);
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);
  virtual bool GetDropFormats(
      int* formats, std::set<OSExchangeData::CustomFormat>* custom_formats);
  virtual bool AreDropTypesRequired();
  virtual bool CanDrop(const OSExchangeData& data);
  virtual void OnDragEntered(const views::DropTargetEvent& event);
  virtual int OnDragUpdated(const views::DropTargetEvent& event);
  virtual void OnDragExited();
  virtual int OnPerformDrop(const views::DropTargetEvent& event);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // BrowserBubble::Delegate methods.
  virtual void BubbleBrowserWindowMoved(BrowserBubble* bubble);
  virtual void BubbleBrowserWindowClosing(BrowserBubble* bubble);
  virtual void BubbleGotFocus(BrowserBubble* bubble);
  virtual void BubbleLostFocus(BrowserBubble* bubble,
                               gfx::NativeView focused_view);

  // Overridden from views::ViewMenuDelegate:
  virtual void RunMenu(View* source, const gfx::Point& pt);

  // Overridden from views::DragController:
  virtual void WriteDragData(View* sender,
                             int press_x,
                             int press_y,
                             OSExchangeData* data);
  virtual int GetDragOperations(View* sender, int x, int y);
  virtual bool CanStartDrag(View* sender,
                            int press_x,
                            int press_y,
                            int x,
                            int y);

  // Overridden from ResizeGripper::ResizeGripperDelegate:
  virtual void OnResize(int resize_amount, bool done_resizing);

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  // Hide the current popup.
  void HidePopup();

  // Simulate a click on a browser action button.  This should only be
  // used by unit tests.
  void TestExecuteBrowserAction(int index);

  // Retrieve the current popup.  This should only be used by unit tests.
  ExtensionPopup* TestGetPopup() { return popup_; }

 private:
  // ExtensionToolbarModel::Observer implementation.
  virtual void BrowserActionAdded(Extension* extension, int index);
  virtual void BrowserActionRemoved(Extension* extension);
  virtual void BrowserActionMoved(Extension* extension, int index);

  // Closes the overflow menu if open.
  void CloseOverflowMenu();

  // Takes a width in pixels, calculates how many icons fit within that space
  // (up to the maximum number of icons in our vector) and shaves off the
  // excess pixels. |allow_shrink_to_minimum| specifies whether this function
  // clamps the size down further (down to ContainerMinSize()) if there is not
  // room for even one icon. When determining how large the container should be
  // this should be |true|. When determining where to place items, such as the
  // drop indicator, this should be |false|.
  int ClampToNearestIconCount(int pixels, bool allow_shrink_to_minimum) const;

  // Calculates the width of the container area NOT used to show the icons (the
  // controls to the left and to the right of the icons).
  int WidthOfNonIconArea() const;

  // Given a number of |icons| return the amount of pixels needed to draw it,
  // including the controls (chevron if visible and resize gripper).
  int IconCountToWidth(int icons) const;

  // Returns the absolute minimum size you can shrink the container down to and
  // still show it. We account for the chevron and the resize gripper, but not
  // all the padding that we normally show if there are icons.
  int ContainerMinSize() const;

  // Returns how many browser actions are visible.
  size_t VisibleBrowserActions() const;

  // The vector of browser actions (icons/image buttons for each action).
  std::vector<BrowserActionView*> browser_action_views_;

  NotificationRegistrar registrar_;

  Profile* profile_;

  // The toolbar that owns us.
  ToolbarView* toolbar_;

  // The current popup and the button it came from.  NULL if no popup.
  ExtensionPopup* popup_;

  // The button that triggered the current popup (just a reference to a button
  // from browser_action_views_).
  BrowserActionButton* popup_button_;

  // The model that tracks the order of the toolbar icons.
  ExtensionToolbarModel* model_;

  // The current size of the container.
  gfx::Size container_size_;

  // The resize gripper for the container.
  views::ResizeGripper* resize_gripper_;

  // The chevron for accessing the overflow items.
  views::MenuButton* chevron_;

  // The menu to show for the overflow button (chevron).
  scoped_ptr<BrowserActionOverflowMenuController> overflow_menu_;

  // The animation that happens when the container snaps to place.
  scoped_ptr<SlideAnimation> resize_animation_;

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

  ScopedRunnableMethodFactory<BrowserActionsContainer> task_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionsContainer);
};

#endif  // CHROME_BROWSER_VIEWS_BROWSER_ACTIONS_CONTAINER_H_
