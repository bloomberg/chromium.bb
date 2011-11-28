// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_instructions_view.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_views.h"
#include "chrome/browser/ui/views/detachable_toolbar_view.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/views/controls/button/button.h"
#include "views/context_menu_controller.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/drag_controller.h"

class BookmarkContextMenu;
class Browser;
class PageNavigator;

namespace ui {
class SlideAnimation;
}

namespace views {
class CustomButton;
class MenuButton;
class MenuItemView;
class TextButton;
}

// BookmarkBarView renders the BookmarkModel.  Each starred entry on the
// BookmarkBar is rendered as a MenuButton. An additional MenuButton aligned to
// the right allows the user to quickly see recently starred entries.
//
// BookmarkBarView shows the bookmarks from a specific Profile. BookmarkBarView
// waits until the HistoryService for the profile has been loaded before
// creating the BookmarkModel.
class BookmarkBarView : public DetachableToolbarView,
                        public BookmarkModelObserver,
                        public views::ViewMenuDelegate,
                        public views::ButtonListener,
                        public content::NotificationObserver,
                        public views::ContextMenuController,
                        public views::DragController,
                        public ui::AnimationDelegate,
                        public BookmarkMenuController::Observer,
                        public BookmarkBarInstructionsView::Delegate {
 public:
  // The internal view class name.
  static const char kViewClassName[];

  // Constants used in Browser View, as well as here.
  // How inset the bookmarks bar is when displayed on the new tab page.
  static const int kNewtabHorizontalPadding;
  static const int kNewtabVerticalPadding;

  // Maximum size of buttons on the bookmark bar.
  static const int kMaxButtonWidth;

  explicit BookmarkBarView(Browser* browser);
  virtual ~BookmarkBarView();

  // Returns the current browser.
  Browser* browser() const { return browser_; }

  // Sets the PageNavigator that is used when the user selects an entry on
  // the bookmark bar.
  void SetPageNavigator(PageNavigator* navigator);

  // Sets whether the containing browser is showing an infobar.  This affects
  // layout during animation.
  void set_infobar_visible(bool infobar_visible) {
    infobar_visible_ = infobar_visible;
  }

  // Changes the state of the bookmark bar.
  void SetBookmarkBarState(BookmarkBar::State state,
                           BookmarkBar::AnimateChangeType animate_type);

  // How much we want the bookmark bar to overlap the toolbar.  If |return_max|
  // is true, we return the maximum overlap rather than the current overlap.
  int GetToolbarOverlap(bool return_max) const;

  // Whether or not we are animating.
  bool is_animating();

  // If |loc| is over a bookmark button the node is returned corresponding to
  // the button and |model_start_index| is set to 0. If a overflow button is
  // showing and |loc| is over the overflow button, the bookmark bar node is
  // returned and |model_start_index| is set to the index of the first node
  // contained in the overflow menu.
  const BookmarkNode* GetNodeForButtonAtModelIndex(const gfx::Point& loc,
                                                   int* model_start_index);

  // Returns the MenuButton for node.
  views::MenuButton* GetMenuButtonForNode(const BookmarkNode* node);

  // Returns the position to anchor the menu for |button| at.
  void GetAnchorPositionForButton(
      views::MenuButton* button,
      views::MenuItemView::AnchorPosition* anchor);

  // Returns the button responsible for showing bookmarks in the other bookmark
  // folder.
  views::MenuButton* other_bookmarked_button() const {
    return other_bookmarked_button_;
  }

  // Returns the button used when not all the items on the bookmark bar fit.
  views::MenuButton* overflow_button() const { return overflow_button_; }

  // Returns the active MenuItemView, or NULL if a menu isn't showing.
  views::MenuItemView* GetMenu();

  // Returns the context menu, or null if one isn't showing.
  views::MenuItemView* GetContextMenu();

  // Returns the drop MenuItemView, or NULL if a menu isn't showing.
  views::MenuItemView* GetDropMenu();

  // If a button is currently throbbing, it is stopped. If immediate is true
  // the throb stops immediately, otherwise it stops after a couple more
  // throbs.
  void StopThrobbing(bool immediate);

  // Returns the tooltip text for the specified url and title. The returned
  // text is clipped to fit within the bounds of the monitor.
  //
  // Note that we adjust the direction of both the URL and the title based on
  // the locale so that pure LTR strings are displayed properly in RTL locales.
  static string16 CreateToolTipForURLAndTitle(const gfx::Point& screen_loc,
                                              const GURL& url,
                                              const string16& title,
                                              Profile* profile);

  // DetachableToolbarView methods:
  virtual bool IsDetached() const OVERRIDE;
  virtual double GetAnimationValue() const OVERRIDE;
  virtual int GetToolbarOverlap() const OVERRIDE;

  // View methods:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual gfx::Size GetMinimumSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(bool
                                    is_add,
                                    View* parent,
                                    View* child) OVERRIDE;
  virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE;
  virtual bool GetDropFormats(
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats) OVERRIDE;
  virtual bool AreDropTypesRequired() OVERRIDE;
  virtual bool CanDrop(const ui::OSExchangeData& data) OVERRIDE;
  virtual void OnDragEntered(const views::DropTargetEvent& event) OVERRIDE;
  virtual int OnDragUpdated(const views::DropTargetEvent& event) OVERRIDE;
  virtual void OnDragExited() OVERRIDE;
  virtual int OnPerformDrop(const views::DropTargetEvent& event) OVERRIDE;
  virtual void ShowContextMenu(const gfx::Point& p,
                               bool is_mouse_gesture) OVERRIDE;
  virtual void OnThemeChanged() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;

  // AccessiblePaneView methods:
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // SlideAnimationDelegate implementation.
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

  // BookmarkMenuController::Observer
  virtual void BookmarkMenuDeleted(
      BookmarkMenuController* controller) OVERRIDE;

  // BookmarkBarInstructionsView::Delegate.
  virtual void ShowImportDialog() OVERRIDE;

  // BookmarkModelObserver:
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) OVERRIDE;
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) OVERRIDE;
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                          const BookmarkNode* node) OVERRIDE;

  // DragController:
  virtual void WriteDragDataForView(views::View* sender,
                                    const gfx::Point& press_pt,
                                    ui::OSExchangeData* data) OVERRIDE;
  virtual int GetDragOperationsForView(views::View* sender,
                                       const gfx::Point& p) OVERRIDE;
  virtual bool CanStartDragForView(views::View* sender,
                                   const gfx::Point& press_pt,
                                   const gfx::Point& p) OVERRIDE;

  // ViewMenuDelegate:
  virtual void RunMenu(views::View* view, const gfx::Point& pt) OVERRIDE;

  // ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // ContextMenuController
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& p,
                                      bool is_mouse_gesture) OVERRIDE;

  // NotificationService:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // If true we're running tests. This short circuits a couple of animations.
  static bool testing_;

 private:
  class ButtonSeparatorView;
  struct DropInfo;
  struct DropLocation;

  friend class BookmarkBarViewEventTestBase;
  FRIEND_TEST_ALL_PREFIXES(BookmarkBarViewTest, SwitchProfile);

  // Used to identify what the user is dropping onto.
  enum DropButtonType {
    DROP_BOOKMARK,
    DROP_OTHER_FOLDER,
    DROP_OVERFLOW
  };

  // Creates recent bookmark button and when visible button as well as
  // calculating the preferred height.
  void Init();

  // NOTE: unless otherwise stated all methods that take an int for an index are
  // in terms of the bookmark bar view. Typically the view index and model index
  // are the same, but they may differ during animations or drag and drop.
  //
  // It's easy to get the mapping wrong. For this reason all these methods are
  // private.

  // Returns the number of buttons corresponding to starred urls/folders. This
  // is equivalent to the number of children the bookmark bar node from the
  // bookmark bar model has.
  int GetBookmarkButtonCount();

  // Returns the button at the specified index.
  views::TextButton* GetBookmarkButton(int index);

  // Returns the index of the first hidden bookmark button. If all buttons are
  // visible, this returns GetBookmarkButtonCount().
  int GetFirstHiddenNodeIndex();

  // Creates the button showing the other bookmarked items.
  views::MenuButton* CreateOtherBookmarkedButton();

  // Creates the button used when not all bookmark buttons fit.
  views::MenuButton* CreateOverflowButton();

  // Creates the button for rendering the specified bookmark node.
  views::View* CreateBookmarkButton(const BookmarkNode* node);

  // Configures the button from the specified node. This sets the text,
  // and icon.
  void ConfigureButton(const BookmarkNode* node, views::TextButton* button);

  // Implementation for BookmarkNodeAddedImpl.
  void BookmarkNodeAddedImpl(BookmarkModel* model,
                             const BookmarkNode* parent,
                             int index);

  // Implementation for BookmarkNodeRemoved.
  void BookmarkNodeRemovedImpl(BookmarkModel* model,
                               const BookmarkNode* parent,
                               int index);

  // If the node is a child of the root node, the button is updated
  // appropriately.
  void BookmarkNodeChangedImpl(BookmarkModel* model, const BookmarkNode* node);

  // Shows the menu used during drag and drop for the specified node.
  void ShowDropFolderForNode(const BookmarkNode* node);

  // Cancels the timer used to show a drop menu.
  void StopShowFolderDropMenuTimer();

  // Stars the timer used to show a drop menu for node.
  void StartShowFolderDropMenuTimer(const BookmarkNode* node);

  // Calculates the location for the drop in |location|.
  void CalculateDropLocation(const views::DropTargetEvent& event,
                             const BookmarkNodeData& data,
                             DropLocation* location);

  // Writes a BookmarkNodeData for node to data.
  void WriteBookmarkDragData(const BookmarkNode* node,
                             ui::OSExchangeData* data);

  // This determines which view should throb and starts it
  // throbbing (e.g when the bookmark bubble is showing).
  // If |overflow_only| is true, start throbbing only if |node| is hidden in
  // the overflow menu.
  void StartThrobbing(const BookmarkNode* node, bool overflow_only);

  // Returns the view to throb when a node is removed. |parent| is the parent of
  // the node that was removed, and |old_index| the index of the node that was
  // removed.
  views::CustomButton* DetermineViewToThrobFromRemove(
      const BookmarkNode* parent,
      int old_index);

  // Updates the colors for all the child objects in the bookmarks bar.
  void UpdateColors();

  // Updates the visibility of |other_bookmarked_button_| and
  // |bookmarks_separator_view_|.
  void UpdateOtherBookmarksVisibility();

  // This method computes the bounds for the bookmark bar items. If
  // |compute_bounds_only| = TRUE, the bounds for the items are just computed,
  // but are not set. This mode is used by GetPreferredSize() to obtain the
  // desired bounds. If |compute_bounds_only| = FALSE, the bounds are set.
  gfx::Size LayoutItems(bool compute_bounds_only);

  content::NotificationRegistrar registrar_;

  // Used for opening urls.
  PageNavigator* page_navigator_;

  // Model providing details as to the starred entries/folders that should be
  // shown. This is owned by the Profile.
  BookmarkModel* model_;

  // Used to manage showing a Menu, either for the most recently bookmarked
  // entries, or for the starred folder.
  BookmarkMenuController* bookmark_menu_;

  // Used when showing a menu for drag and drop. That is, if the user drags
  // over a folder this becomes non-null and manages the menu showing the
  // contents of the node.
  BookmarkMenuController* bookmark_drop_menu_;

  // If non-NULL we're showing a context menu for one of the items on the
  // bookmark bar.
  scoped_ptr<BookmarkContextMenu> context_menu_;

  // Shows the other bookmark entries.
  views::MenuButton* other_bookmarked_button_;

  // Task used to delay showing of the drop menu.
  base::WeakPtrFactory<BookmarkBarView> show_folder_method_factory_;

  // Used to track drops on the bookmark bar view.
  scoped_ptr<DropInfo> drop_info_;

  // Visible if not all the bookmark buttons fit.
  views::MenuButton* overflow_button_;

  // BookmarkBarInstructionsView that is visible if there are no bookmarks on
  // the bookmark bar.
  views::View* instructions_;

  ButtonSeparatorView* bookmarks_separator_view_;

  // Owning browser.
  Browser* browser_;

  // True if the owning browser is showing an infobar.
  bool infobar_visible_;

  // Animation controlling showing and hiding of the bar.
  scoped_ptr<ui::SlideAnimation> size_animation_;

  // If the bookmark bubble is showing, this is the visible ancestor of the URL.
  // The visible ancestor is either the other_bookmarked_button_,
  // overflow_button_ or a button on the bar.
  views::CustomButton* throbbing_view_;

  BookmarkBar::State bookmark_bar_state_;

  // Are we animating to or from the detached state?
  bool animating_detached_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BAR_VIEW_H_
