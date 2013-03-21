// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_BAR_GTK_H_
#define CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_BAR_GTK_H_

#include <gtk/gtk.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_instructions_delegate.h"
#include "chrome/browser/ui/bookmarks/bookmark_context_menu_controller.h"
#include "chrome/browser/ui/gtk/menu_bar_helper.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/animation/animation.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

class BookmarkBarInstructionsGtk;
class BookmarkMenuController;
class Browser;
class BrowserWindowGtk;
class GtkThemeService;
class MenuGtk;
class TabstripOriginProvider;

namespace content {
class PageNavigator;
}

class BookmarkBarGtk : public ui::AnimationDelegate,
                       public BookmarkModelObserver,
                       public MenuBarHelper::Delegate,
                       public content::NotificationObserver,
                       public chrome::BookmarkBarInstructionsDelegate,
                       public BookmarkContextMenuControllerDelegate {
 public:
  BookmarkBarGtk(BrowserWindowGtk* window,
                 Browser* browser,
                 TabstripOriginProvider* tabstrip_origin_provider);
  virtual ~BookmarkBarGtk();

  // Returns the current browser.
  Browser* browser() const { return browser_; }

  // Returns the top level widget.
  GtkWidget* widget() const { return event_box_.get(); }

  // Sets the PageNavigator that is used when the user selects an entry on
  // the bookmark bar.
  void SetPageNavigator(content::PageNavigator* navigator);

  // Create the contents of the bookmark bar.
  void Init();

  // Changes the state of the bookmark bar.
  void SetBookmarkBarState(BookmarkBar::State state,
                           BookmarkBar::AnimateChangeType animate_type);

  // Get the current height of the bookmark bar.
  int GetHeight();

  // Returns true if the bookmark bar is showing an animation.
  bool IsAnimating();

  // ui::AnimationDelegate implementation --------------------------------------
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

  // MenuBarHelper::Delegate implementation ------------------------------------
  virtual void PopupForButton(GtkWidget* button) OVERRIDE;
  virtual void PopupForButtonNextTo(GtkWidget* button,
                                    GtkMenuDirectionType dir) OVERRIDE;

  // BookmarkContextMenuController::Delegate implementation --------------------
  virtual void CloseMenu() OVERRIDE;

  const ui::Animation* animation() { return &slide_animation_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(BookmarkBarGtkUnittest, DisplaysHelpMessageOnEmpty);
  FRIEND_TEST_ALL_PREFIXES(BookmarkBarGtkUnittest,
                           HidesHelpMessageWithBookmark);
  FRIEND_TEST_ALL_PREFIXES(BookmarkBarGtkUnittest, BuildsButtons);

  // Change the visibility of the bookmarks bar. (Starts out hidden, per GTK's
  // default behaviour). There are three visiblity states:
  //
  //   Showing    - bookmark bar is fully visible.
  //   Hidden     - bookmark bar is hidden except for a few pixels that give
  //                extra padding to the bottom of the toolbar. Buttons are not
  //                clickable.
  //   Fullscreen - bookmark bar is fully hidden.
  void Show(BookmarkBar::State old_state,
            BookmarkBar::AnimateChangeType animate_type);
  void Hide(BookmarkBar::State old_state,
            BookmarkBar::AnimateChangeType animate_type);

  // Calculate maximum height of bookmark bar.
  void CalculateMaxHeight();

  // Helper function which generates GtkToolItems for |bookmark_toolbar_|.
  void CreateAllBookmarkButtons();

  // Sets the visibility of the instructional text based on whether there are
  // any bookmarks in the bookmark bar node.
  void SetInstructionState();

  // Sets the visibility of the overflow chevron.
  void SetChevronState();

  // Shows or hides the other bookmarks button depending on whether there are
  // bookmarks in it.
  void UpdateOtherBookmarksVisibility();

  // Destroys all the bookmark buttons in the GtkToolbar.
  void RemoveAllButtons();

  // Adds the "other bookmarks" and overflow buttons.
  void AddCoreButtons();

  // Removes and recreates all buttons in the bar.
  void ResetButtons();

  // Returns the number of buttons corresponding to starred urls/folders. This
  // is equivalent to the number of children the bookmark bar node from the
  // bookmark bar model has.
  int GetBookmarkButtonCount();

  // Returns LAUNCH_DETACHED_BAR or LAUNCH_ATTACHED_BAR based on detached state.
  bookmark_utils::BookmarkLaunchLocation GetBookmarkLaunchLocation() const;

  // Set the appearance of the overflow button appropriately (either chromium
  // style or GTK style).
  void SetOverflowButtonAppearance();

  // Returns the index of the first bookmark that is not visible on the bar.
  // Returns -1 if they are all visible.
  // |extra_space| is how much extra space to give the toolbar during the
  // calculation (for the purposes of determining if ditching the chevron
  // would be a good idea).
  // If non-NULL, |showing_folders| will be packed with all the folders that are
  // showing on the bar.
  int GetFirstHiddenBookmark(int extra_space,
                             std::vector<GtkWidget*>* showing_folders);

  // Update the detached state (either enable or disable it, or do nothing).
  void UpdateDetachedState(BookmarkBar::State old_state);

  // Turns on or off the app_paintable flag on |event_box_|, depending on our
  // state.
  void UpdateEventBoxPaintability();

  // Queue a paint on the event box.
  void PaintEventBox();

  // Finds the size of the current web contents, if it exists and sets |size|
  // to the correct value. Returns false if there isn't a WebContents, a
  // condition that can happen during testing.
  bool GetWebContentsSize(gfx::Size* size);

  // Connects to the "size-allocate" signal on the given widget, and causes it
  // to throb after allocation. This is called when a new item is added to the
  // bar. We can't call StartThrobbing directly because we don't know if it's
  // visible or not until after the widget is allocated.
  void StartThrobbingAfterAllocation(GtkWidget* item);

  // Used by StartThrobbingAfterAllocation.
  CHROMEGTK_CALLBACK_1(BookmarkBarGtk, void, OnItemAllocate, GtkAllocation*);

  // Makes the appropriate widget on the bookmark bar stop throbbing
  // (a folder, the overflow chevron, or nothing).
  void StartThrobbing(const BookmarkNode* node);

  // Set |throbbing_widget_| to |widget|. Also makes sure that
  // |throbbing_widget_| doesn't become stale.
  void SetThrobbingWidget(GtkWidget* widget);

  // An item has been dragged over the toolbar, update the drag context
  // and toolbar UI appropriately.
  gboolean ItemDraggedOverToolbar(
      GdkDragContext* context, int index, guint time);

  // When dragging in the middle of a folder, assume the user wants to drop
  // on the folder. Towards the edges, assume the user wants to drop on the
  // toolbar. This makes it possible to drop between two folders. This function
  // returns the index on the toolbar the drag should target, or -1 if the
  // drag should hit the folder.
  int GetToolbarIndexForDragOverFolder(GtkWidget* button, gint x);

  void ClearToolbarDropHighlighting();

  // Overridden from BookmarkModelObserver:

  // Invoked when the bookmark model has finished loading. Creates a button
  // for each of the children of the root node from the model.
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;

  // Invoked when the model is being deleted.
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;

  // Invoked when a node has moved.
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
  // Invoked when a favicon has finished loading.
  virtual void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                          const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  GtkWidget* CreateBookmarkButton(const BookmarkNode* node);
  GtkToolItem* CreateBookmarkToolItem(const BookmarkNode* node);

  void ConnectFolderButtonEvents(GtkWidget* widget, bool is_tool_item);

  // Finds the BookmarkNode from the model associated with |button|.
  const BookmarkNode* GetNodeForToolButton(GtkWidget* button);

  // Creates and displays a popup menu for BookmarkNode |node|.
  void PopupMenuForNode(GtkWidget* sender, const BookmarkNode* node,
                        GdkEventButton* event);

  // GtkButton callbacks.
  CHROMEGTK_CALLBACK_1(BookmarkBarGtk, gboolean, OnButtonPressed,
                       GdkEventButton*);
  CHROMEGTK_CALLBACK_0(BookmarkBarGtk, void, OnClicked);
  CHROMEGTK_CALLBACK_1(BookmarkBarGtk, void, OnButtonDragBegin,
                       GdkDragContext*);
  CHROMEGTK_CALLBACK_1(BookmarkBarGtk, void, OnButtonDragEnd, GdkDragContext*);
  CHROMEGTK_CALLBACK_4(BookmarkBarGtk, void, OnButtonDragGet,
                       GdkDragContext*, GtkSelectionData*, guint, guint);

  // GtkButton callbacks for folder buttons.
  CHROMEGTK_CALLBACK_0(BookmarkBarGtk, void, OnFolderClicked);

  // GtkToolbar callbacks.
  CHROMEGTK_CALLBACK_4(BookmarkBarGtk, gboolean, OnToolbarDragMotion,
                       GdkDragContext*, gint, gint, guint);
  CHROMEGTK_CALLBACK_1(BookmarkBarGtk, void, OnToolbarSizeAllocate,
                       GtkAllocation*);

  // Used for both folder buttons and the toolbar.
  CHROMEGTK_CALLBACK_6(BookmarkBarGtk, void, OnDragReceived,
                       GdkDragContext*, gint, gint, GtkSelectionData*,
                       guint, guint);
  CHROMEGTK_CALLBACK_2(BookmarkBarGtk, void, OnDragLeave,
                       GdkDragContext*, guint);

  // Used for folder buttons.
  CHROMEGTK_CALLBACK_4(BookmarkBarGtk, gboolean, OnFolderDragMotion,
                       GdkDragContext*, gint, gint, guint);

  // GtkEventBox callbacks.
  CHROMEGTK_CALLBACK_1(BookmarkBarGtk, gboolean, OnEventBoxExpose,
                       GdkEventExpose*);
  CHROMEGTK_CALLBACK_0(BookmarkBarGtk, void, OnEventBoxDestroy);

  // Callbacks on our parent widget.
  CHROMEGTK_CALLBACK_1(BookmarkBarGtk, void, OnParentSizeAllocate,
                       GtkAllocation*);

  // |throbbing_widget_| callback.
  CHROMEGTK_CALLBACK_0(BookmarkBarGtk, void, OnThrobbingWidgetDestroy);

  // Overriden from chrome::BookmarkBarInstructionsDelegate:
  virtual void ShowImportDialog() OVERRIDE;

  // Updates the drag&drop state when |edit_bookmarks_enabled_| changes.
  void OnEditBookmarksEnabledChanged();

  // Used for opening urls.
  content::PageNavigator* page_navigator_;

  Browser* browser_;
  BrowserWindowGtk* window_;

  // Provides us with the offset into the background theme image.
  TabstripOriginProvider* tabstrip_origin_provider_;

  // Model providing details as to the starred entries/folders that should be
  // shown. This is owned by the Profile.
  BookmarkModel* model_;

  // Contains |bookmark_hbox_|. Event box exists to prevent leakage of
  // background color from the toplevel application window's GDK window.
  ui::OwnedWidgetGtk event_box_;

  // Used to detached the bookmark bar when on the NTP.
  GtkWidget* ntp_padding_box_;

  // Used to paint the background of the bookmark bar when in detached mode.
  GtkWidget* paint_box_;

  // Used to position all children.
  GtkWidget* bookmark_hbox_;

  // Alignment widget that is visible if there are no bookmarks on
  // the bookmar bar.
  GtkWidget* instructions_;

  // BookmarkBarInstructionsGtk that holds the label and the link for importing
  // bookmarks when there are no bookmarks on the bookmark bar.
  scoped_ptr<BookmarkBarInstructionsGtk> instructions_gtk_;

  // GtkToolbar which contains all the bookmark buttons.
  ui::OwnedWidgetGtk bookmark_toolbar_;

  // The button that shows extra bookmarks that don't fit on the bookmark
  // bar.
  GtkWidget* overflow_button_;

  // A separator between the main bookmark bar area and
  // |other_bookmarks_button_|.
  GtkWidget* other_bookmarks_separator_;

  // The other bookmarks button.
  GtkWidget* other_bookmarks_button_;

  // Padding for the other bookmarks button.
  GtkWidget* other_padding_;

  // The BookmarkNode from the model being dragged. NULL when we aren't
  // dragging.
  const BookmarkNode* dragged_node_;

  // The visual representation that follows the cursor during drags.
  GtkWidget* drag_icon_;

  // We create a GtkToolbarItem from |dragged_node_| ;or display.
  GtkToolItem* toolbar_drop_item_;

  // Theme provider for building buttons.
  GtkThemeService* theme_service_;

  // Whether we should show the instructional text in the bookmark bar.
  bool show_instructions_;

  MenuBarHelper menu_bar_helper_;

  // The last displayed right click menu, or NULL if no menus have been
  // displayed yet.
  // The controller.
  scoped_ptr<BookmarkContextMenuController> current_context_menu_controller_;
  // The view.
  scoped_ptr<MenuGtk> current_context_menu_;

  // The last displayed left click menu, or NULL if no menus have been
  // displayed yet.
  scoped_ptr<BookmarkMenuController> current_menu_;

  ui::SlideAnimation slide_animation_;

  // Used to optimize out |bookmark_toolbar_| size-allocate events we don't
  // need to respond to.
  int last_allocation_width_;

  content::NotificationRegistrar registrar_;

  // The size of the web contents last time we forced a paint. We keep track
  // of this so we don't force too many paints.
  gfx::Size last_web_contents_size_;

  // The last coordinates recorded by OnButtonPress; used to line up the
  // drag icon during bookmark drags.
  gfx::Point last_pressed_coordinates_;

  // The currently throbbing widget. This is NULL if no widget is throbbing.
  // We track it because we only want to allow one widget to throb at a time.
  GtkWidget* throbbing_widget_;

  // Tracks whether bookmarks can be modified.
  BooleanPrefMember edit_bookmarks_enabled_;

  base::WeakPtrFactory<BookmarkBarGtk> weak_factory_;

  BookmarkBar::State bookmark_bar_state_;

  // Maximum height of the bookmark bar.
  int max_height_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_BAR_GTK_H_
