// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARK_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARK_BAR_VIEW_H_
#pragma once

#include <set>

#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/views/bookmark_bar_instructions_view.h"
#include "chrome/browser/ui/views/bookmark_menu_controller_views.h"
#include "chrome/browser/ui/views/detachable_toolbar_view.h"
#include "chrome/common/notification_registrar.h"
#include "ui/base/animation/animation_delegate.h"
#include "views/controls/button/button.h"
#include "views/controls/menu/view_menu_delegate.h"

class Browser;
class PageNavigator;
class PrefService;

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
                        public ProfileSyncServiceObserver,
                        public BookmarkModelObserver,
                        public views::ViewMenuDelegate,
                        public views::ButtonListener,
                        public NotificationObserver,
                        public views::ContextMenuController,
                        public views::DragController,
                        public ui::AnimationDelegate,
                        public BookmarkMenuController::Observer,
                        public BookmarkBarInstructionsView::Delegate {
  friend class ShowFolderMenuTask;

 public:
  // Constants used in Browser View, as well as here.
  // How inset the bookmarks bar is when displayed on the new tab page.
  static const int kNewtabHorizontalPadding;
  static const int kNewtabVerticalPadding;

  // Maximum size of buttons on the bookmark bar.
  static const int kMaxButtonWidth;

  // Interface implemented by controllers/views that need to be notified any
  // time the model changes, typically to cancel an operation that is showing
  // data from the model such as a menu. This isn't intended as a general
  // way to be notified of changes, rather for cases where a controller/view is
  // showing data from the model in a modal like setting and needs to cleanly
  // exit the modal loop if the model changes out from under it.
  //
  // A controller/view that needs this notification should install itself as the
  // ModelChangeListener via the SetModelChangedListener method when shown and
  // reset the ModelChangeListener of the BookmarkBarView when it closes by way
  // of either the SetModelChangedListener method or the
  // ClearModelChangedListenerIfEquals method.
  class ModelChangedListener {
   public:
    virtual ~ModelChangedListener() {}

    // Invoked when the model changes. Should cancel the edit and close any
    // dialogs.
    virtual void ModelChanged() = 0;
  };

  static const int kNewtabBarHeight;

  BookmarkBarView(Profile* profile, Browser* browser);
  virtual ~BookmarkBarView();

  // Resets the profile. This removes any buttons for the current profile and
  // recreates the models.
  void SetProfile(Profile* profile);

  // Returns the current profile.
  Profile* GetProfile() { return profile_; }

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

  // DetachableToolbarView methods:
  virtual bool IsDetached() const;
  virtual bool IsOnTop() const;
  virtual double GetAnimationValue() const;
  virtual int GetToolbarOverlap() const {
    return GetToolbarOverlap(false);
  }

  // View methods:
  virtual gfx::Size GetPreferredSize();
  virtual gfx::Size GetMinimumSize();
  virtual void Layout();
  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);
  virtual void PaintChildren(gfx::Canvas* canvas);
  virtual bool GetDropFormats(
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats);
  virtual bool AreDropTypesRequired();
  virtual bool CanDrop(const ui::OSExchangeData& data);
  virtual void OnDragEntered(const views::DropTargetEvent& event);
  virtual int OnDragUpdated(const views::DropTargetEvent& event);
  virtual void OnDragExited();
  virtual int OnPerformDrop(const views::DropTargetEvent& event);
  virtual void ShowContextMenu(const gfx::Point& p, bool is_mouse_gesture);

  // AccessiblePaneView methods:
  virtual bool IsAccessibleViewTraversable(views::View* view);
  virtual AccessibilityTypes::Role GetAccessibleRole();

  // ProfileSyncServiceObserver method.
  virtual void OnStateChanged();

  // Called when fullscreen mode toggles on or off; this affects our layout.
  void OnFullscreenToggled(bool fullscreen);

  // Sets the model change listener to listener.
  void SetModelChangedListener(ModelChangedListener* listener) {
    model_changed_listener_ = listener;
  }

  // If the ModelChangedListener is listener, ModelChangeListener is set to
  // NULL.
  void ClearModelChangedListenerIfEquals(ModelChangedListener* listener) {
    if (model_changed_listener_ == listener)
      model_changed_listener_ = NULL;
  }

  // Returns the model change listener.
  ModelChangedListener* GetModelChangedListener() {
    return model_changed_listener_;
  }

  // Returns the page navigator.
  PageNavigator* GetPageNavigator() { return page_navigator_; }

  // Returns the model.
  BookmarkModel* GetModel() { return model_; }

  // Returns true if the bookmarks bar preference is set to 'always show'.
  bool IsAlwaysShown() const;

  // True if we're on a page where the bookmarks bar is always visible.
  bool OnNewTabPage() const;

  // How much we want the bookmark bar to overlap the toolbar.  If |return_max|
  // is true, we return the maximum overlap rather than the current overlap.
  int GetToolbarOverlap(bool return_max) const;

  // Whether or not we are animating.
  bool is_animating();

  // SlideAnimationDelegate implementation.
  void AnimationProgressed(const ui::Animation* animation);
  void AnimationEnded(const ui::Animation* animation);

  // BookmarkMenuController::Observer
  virtual void BookmarkMenuDeleted(BookmarkMenuController* controller);

  // Returns the button at the specified index.
  views::TextButton* GetBookmarkButton(int index);

  // Returns the button responsible for showing bookmarks in the other bookmark
  // folder.
  views::MenuButton* other_bookmarked_button() const {
    return other_bookmarked_button_;
  }

  // Returns the active MenuItemView, or NULL if a menu isn't showing.
  views::MenuItemView* GetMenu();

  // Returns the drop MenuItemView, or NULL if a menu isn't showing.
  views::MenuItemView* GetDropMenu();

  // Returns the context menu, or null if one isn't showing.
  views::MenuItemView* GetContextMenu();

  // Returns the button used when not all the items on the bookmark bar fit.
  views::MenuButton* overflow_button() const { return overflow_button_; }

  // If |loc| is over a bookmark button the node is returned corresponding
  // to the button and |start_index| is set to 0. If a overflow button is
  // showing and |loc| is over the overflow button, the bookmark bar node is
  // returned and |start_index| is set to the index of the first node
  // contained in the overflow menu.
  const BookmarkNode* GetNodeForButtonAt(const gfx::Point& loc,
                                         int* start_index);

  // Returns the MenuButton for node.
  views::MenuButton* GetMenuButtonForNode(const BookmarkNode* node);

  // Returns the position to anchor the menu for |button| at, the index of the
  // first child of the node to build the menu from.
  void GetAnchorPositionAndStartIndexForButton(
      views::MenuButton* button,
      views::MenuItemView::AnchorPosition* anchor,
      int* start_index);

  // BookmarkBarInstructionsView::Delegate.
  virtual void ShowImportDialog();

  // If a button is currently throbbing, it is stopped. If immediate is true
  // the throb stops immediately, otherwise it stops after a couple more
  // throbs.
  void StopThrobbing(bool immediate);

  // Returns the number of buttons corresponding to starred urls/groups. This
  // is equivalent to the number of children the bookmark bar node from the
  // bookmark bar model has.
  int GetBookmarkButtonCount();

  // Returns the tooltip text for the specified url and title. The returned
  // text is clipped to fit within the bounds of the monitor.
  //
  // Note that we adjust the direction of both the URL and the title based on
  // the locale so that pure LTR strings are displayed properly in RTL locales.
  static std::wstring CreateToolTipForURLAndTitle(
      const gfx::Point& screen_loc,
      const GURL& url,
      const std::wstring& title,
      Profile* profile);

  // If true we're running tests. This short circuits a couple of animations.
  static bool testing_;

 private:
  class ButtonSeparatorView;
  struct DropInfo;

  // Task that invokes ShowDropFolderForNode when run. ShowFolderDropMenuTask
  // deletes itself once run.
  class ShowFolderDropMenuTask : public Task {
   public:
    ShowFolderDropMenuTask(BookmarkBarView* view, const BookmarkNode* node)
      : view_(view),
        node_(node) {
    }

    void Cancel() {
      view_->show_folder_drop_menu_task_ = NULL;
      view_ = NULL;
    }

    virtual void Run() {
      if (view_) {
        view_->show_folder_drop_menu_task_ = NULL;
        view_->ShowDropFolderForNode(node_);
      }
      // MessageLoop deletes us.
    }

   private:
    BookmarkBarView* view_;
    const BookmarkNode* node_;

    DISALLOW_COPY_AND_ASSIGN(ShowFolderDropMenuTask);
  };

  // Creates recent bookmark button and when visible button as well as
  // calculating the preferred height.
  void Init();

  // Creates the button showing the other bookmarked items.
  views::MenuButton* CreateOtherBookmarkedButton();

  // Creates the button used when not all bookmark buttons fit.
  views::MenuButton* CreateOverflowButton();

  // Invoked when the bookmark bar model has finished loading. Creates a button
  // for each of the children of the root node from the model.
  virtual void Loaded(BookmarkModel* model);

  // Invoked when the model is being deleted.
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model);

  // Invokes added followed by removed.
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index);

  // Notifies ModelChangeListener of change.
  // If the node was added to the root node, a button is created and added to
  // this bookmark bar view.
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index);

  // Implementation for BookmarkNodeAddedImpl.
  void BookmarkNodeAddedImpl(BookmarkModel* model,
                             const BookmarkNode* parent,
                             int index);

  // Notifies ModelChangeListener of change.
  // If the node was a child of the root node, the button corresponding to it
  // is removed.
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node);

  // Implementation for BookmarkNodeRemoved.
  void BookmarkNodeRemovedImpl(BookmarkModel* model,
                               const BookmarkNode* parent,
                               int index);

  // Notifies ModelChangedListener and invokes BookmarkNodeChangedImpl.
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node);

  // If the node is a child of the root node, the button is updated
  // appropriately.
  void BookmarkNodeChangedImpl(BookmarkModel* model, const BookmarkNode* node);

  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node);

  // Invoked when the favicon is available. If the node is a child of the
  // root node, the appropriate button is updated. If a menu is showing, the
  // call is forwarded to the menu to allow for it to update the icon.
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node);

  // DragController method. Determines the node representing sender and invokes
  // WriteDragData to write the actual data.
  virtual void WriteDragData(views::View* sender,
                             const gfx::Point& press_pt,
                             ui::OSExchangeData* data);

  virtual int GetDragOperations(views::View* sender, const gfx::Point& p);

  virtual bool CanStartDrag(views::View* sender,
                            const gfx::Point& press_pt,
                            const gfx::Point& p);

  // Writes a BookmarkNodeData for node to data.
  void WriteDragData(const BookmarkNode* node, ui::OSExchangeData* data);

  // ViewMenuDelegate method. Ends up creating a BookmarkMenuController to
  // show the menu.
  virtual void RunMenu(views::View* view, const gfx::Point& pt);

  // Invoked when a star entry corresponding to a URL on the bookmark bar is
  // pressed. Forwards to the PageNavigator to open the URL.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Invoked for this View, one of the buttons or the 'other' button. Shows the
  // appropriate context menu.
  virtual void ShowContextMenu(views::View* source,
                               const gfx::Point& p,
                               bool is_mouse_gesture);

  // Creates the button for rendering the specified bookmark node.
  views::View* CreateBookmarkButton(const BookmarkNode* node);

  // COnfigures the button from the specified node. This sets the text,
  // and icon.
  void ConfigureButton(const BookmarkNode* node, views::TextButton* button);

  // Used when showing the menu allowing the user to choose when the bar is
  // visible. Return value corresponds to the users preference for when the
  // bar is visible.
  virtual bool IsItemChecked(int id) const;

  // Used when showing the menu allowing the user to choose when the bar is
  // visible. Updates the preferences to match the users choice as appropriate.
  virtual void ExecuteCommand(int id);

  // NotificationService method.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from views::View.
  virtual void OnThemeChanged();

  // If the ModelChangedListener is non-null, ModelChanged is invoked on it.
  void NotifyModelChanged();

  // Shows the menu used during drag and drop for the specified node.
  void ShowDropFolderForNode(const BookmarkNode* node);

  // Cancels the timer used to show a drop menu.
  void StopShowFolderDropMenuTimer();

  // Stars the timer used to show a drop menu for node.
  void StartShowFolderDropMenuTimer(const BookmarkNode* node);

  // Returns the drop operation and index for the drop based on the event
  // and data. Returns ui::DragDropTypes::DRAG_NONE if not a valid location.
  int CalculateDropOperation(const views::DropTargetEvent& event,
                             const BookmarkNodeData& data,
                             int* index,
                             bool* drop_on,
                             bool* is_over_overflow,
                             bool* is_over_other);

  // Returns the index of the first hidden bookmark button. If all buttons are
  // visible, this returns GetBookmarkButtonCount().
  int GetFirstHiddenNodeIndex();

  // This determines which view should throb and starts it
  // throbbing (e.g when the bookmark bubble is showing).
  // If |overflow_only| is true, start throbbing only if |node| is hidden in
  // the overflow menu.
  void StartThrobbing(const BookmarkNode* node, bool overflow_only);

  // Updates the colors for all the child objects in the bookmarks bar.
  void UpdateColors();

  // This method computes the bounds for the bookmark bar items. If
  // |compute_bounds_only| = TRUE, the bounds for the items are just computed,
  // but are not set. This mode is used by GetPreferredSize() to obtain the
  // desired bounds. If |compute_bounds_only| = FALSE, the bounds are set.
  gfx::Size LayoutItems(bool compute_bounds_only);

  // Creates the sync error button and adds it as a child view.
  views::TextButton* CreateSyncErrorButton();

  NotificationRegistrar registrar_;

  Profile* profile_;

  // Used for opening urls.
  PageNavigator* page_navigator_;

  // Model providing details as to the starred entries/groups that should be
  // shown. This is owned by the Profile.
  BookmarkModel* model_;

  // Used to manage showing a Menu, either for the most recently bookmarked
  // entries, or for the a starred group.
  BookmarkMenuController* bookmark_menu_;

  // Used when showing a menu for drag and drop. That is, if the user drags
  // over a group this becomes non-null and manages the menu showing the
  // contents of the node.
  BookmarkMenuController* bookmark_drop_menu_;

  // Shows the other bookmark entries.
  views::MenuButton* other_bookmarked_button_;

  // ModelChangeListener.
  ModelChangedListener* model_changed_listener_;

  // Task used to delay showing of the drop menu.
  ShowFolderDropMenuTask* show_folder_drop_menu_task_;

  // Used to track drops on the bookmark bar view.
  scoped_ptr<DropInfo> drop_info_;

  // The sync re-login indicator which appears when the user needs to re-enter
  // credentials in order to continue syncing.
  views::TextButton* sync_error_button_;

  // A pointer to the ProfileSyncService instance if one exists.
  ProfileSyncService* sync_service_;

  // Visible if not all the bookmark buttons fit.
  views::MenuButton* overflow_button_;

  // BookmarkBarInstructionsView that is visible if there are no bookmarks on
  // the bookmark bar.
  views::View* instructions_;

  ButtonSeparatorView* bookmarks_separator_view_;

  // Owning browser. This is NULL during testing.
  Browser* browser_;

  // True if the owning browser is showing an infobar.
  bool infobar_visible_;

  // Animation controlling showing and hiding of the bar.
  scoped_ptr<ui::SlideAnimation> size_animation_;

  // If the bookmark bubble is showing, this is the visible ancestor of the URL.
  // The visible ancestor is either the other_bookmarked_button_,
  // overflow_button_ or a button on the bar.
  views::CustomButton* throbbing_view_;

  // Background for extension toolstrips.
  SkBitmap toolstrip_background_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARK_BAR_VIEW_H_
