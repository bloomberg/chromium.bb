// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BOOKMARK_BAR_GTK_H_
#define CHROME_BROWSER_GTK_BOOKMARK_BAR_GTK_H_

#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "app/slide_animation.h"
#include "base/gfx/size.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/gtk/bookmark_bar_instructions_gtk.h"
#include "chrome/browser/gtk/menu_bar_helper.h"
#include "chrome/browser/gtk/view_id_util.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/owned_widget_gtk.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class BookmarkContextMenuGtk;
class BookmarkMenuController;
class Browser;
class BrowserWindowGtk;
class CustomContainerButton;
class MenuGtk;
class PageNavigator;
class Profile;
class TabstripOriginProvider;
struct GtkThemeProvider;

class BookmarkBarGtk : public AnimationDelegate,
                       public ProfileSyncServiceObserver,
                       public BookmarkModelObserver,
                       public MenuBarHelper::Delegate,
                       public NotificationObserver,
                       public BookmarkBarInstructionsGtk::Delegate {
  FRIEND_TEST(BookmarkBarGtkUnittest, DisplaysHelpMessageOnEmpty);
  FRIEND_TEST(BookmarkBarGtkUnittest, HidesHelpMessageWithBookmark);
  FRIEND_TEST(BookmarkBarGtkUnittest, BuildsButtons);
 public:
  explicit BookmarkBarGtk(BrowserWindowGtk* window,
                          Profile* profile, Browser* browser,
                          TabstripOriginProvider* tabstrip_origin_provider);
  virtual ~BookmarkBarGtk();

  // Resets the profile. This removes any buttons for the current profile and
  // recreates the models.
  void SetProfile(Profile* profile);

  // Returns the current profile.
  Profile* GetProfile() { return profile_; }

  // Returns the current browser.
  Browser* browser() const { return browser_; }

  // Returns the top level widget.
  GtkWidget* widget() const { return event_box_.get(); }

  // Sets the PageNavigator that is used when the user selects an entry on
  // the bookmark bar.
  void SetPageNavigator(PageNavigator* navigator);

  // Create the contents of the bookmark bar.
  void Init(Profile* profile);

  // Whether the current page is the New Tag Page (which requires different
  // rendering).
  bool OnNewTabPage();

  // Change the visibility of the bookmarks bar. (Starts out hidden, per GTK's
  // default behaviour). There are three visiblity states:
  //
  //   Showing    - bookmark bar is fully visible.
  //   Hidden     - bookmark bar is hidden except for a few pixels that give
  //                extra padding to the bottom of the toolbar. Buttons are not
  //                clickable.
  //   Fullscreen - bookmark bar is fully hidden.
  void Show(bool animate);
  void Hide(bool animate);
  void EnterFullscreen();

  // Get the current height of the bookmark bar.
  int GetHeight();

  // Returns true if the bookmark bar is showing an animation.
  bool IsAnimating();

  // Returns true if the bookmarks bar preference is set to 'always show'.
  bool IsAlwaysShown();

  // AnimationDelegate implementation ------------------------------------------
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  // MenuBarHelper::Delegate implementation ------------------------------------
  virtual void PopupForButton(GtkWidget* button);
  virtual void PopupForButtonNextTo(GtkWidget* button,
                                    GtkMenuDirectionType dir);

  // The NTP needs to have access to this.
  static const int kBookmarkBarNTPHeight;

 private:
  // Helper function which generates GtkToolItems for |bookmark_toolbar_|.
  void CreateAllBookmarkButtons();

  // Sets the visibility of the instructional text based on whether there are
  // any bookmarks in the bookmark bar node.
  void SetInstructionState();

  // Sets the visibility of the overflow chevron.
  void SetChevronState();

  // Helper function which destroys all the bookmark buttons in the GtkToolbar.
  void RemoveAllBookmarkButtons();

  // Returns the number of buttons corresponding to starred urls/groups. This
  // is equivalent to the number of children the bookmark bar node from the
  // bookmark bar model has.
  int GetBookmarkButtonCount();

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

  // Returns true if the bookmark bar should be floating on the page (for
  // NTP).
  bool ShouldBeFloating();
  // Update the floating state (either enable or disable it, or do nothing).
  void UpdateFloatingState();

  // Turns on or off the app_paintable flag on |event_box_|, depending on our
  // state.
  void UpdateEventBoxPaintability();

  // Queue a paint on the event box.
  void PaintEventBox();

  // Finds the size of the current tab contents. If the size cannot be found,
  // DCHECKs and returns false. Otherwise, sets |size| to the correct value.
  bool GetTabContentsSize(gfx::Size* size);

  // Makes the appropriate widget on the bookmark bar stop throbbing
  // (a folder, the overflow chevron, or nothing).
  void StartThrobbing(const BookmarkNode* node);

  // Set |throbbing_widget_| to |widget|. Also makes sure that
  // |throbbing_widget_| doesn't become stale.
  void SetThrobbingWidget(GtkWidget* widget);

  // Overridden from BookmarkModelObserver:

  // Invoked when the bookmark model has finished loading. Creates a button
  // for each of the children of the root node from the model.
  virtual void Loaded(BookmarkModel* model);

  // Invoked when the model is being deleted.
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model);

  // Invoked when a node has moved.
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index);
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index);
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node);
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node);
  // Invoked when a favicon has finished loading.
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node);
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  GtkWidget* CreateBookmarkButton(const BookmarkNode* node);
  GtkToolItem* CreateBookmarkToolItem(const BookmarkNode* node);

  void ConnectFolderButtonEvents(GtkWidget* widget);

  // Finds the BookmarkNode from the model associated with |button|.
  const BookmarkNode* GetNodeForToolButton(GtkWidget* button);

  // Creates and displays a popup menu for BookmarkNode |node|.
  void PopupMenuForNode(GtkWidget* sender, const BookmarkNode* node,
                        GdkEventButton* event);

  // GtkButton callbacks.
  static gboolean OnButtonPressed(GtkWidget* sender,
                                  GdkEventButton* event,
                                  BookmarkBarGtk* bar);
  static gboolean OnSyncErrorButtonPressed(GtkWidget* sender,
                                           GdkEventButton* event,
                                           BookmarkBarGtk* bar);
  static void OnClicked(GtkWidget* sender,
                        BookmarkBarGtk* bar);
  static void OnButtonDragBegin(GtkWidget* widget,
                                GdkDragContext* drag_context,
                                BookmarkBarGtk* bar);
  static void OnButtonDragEnd(GtkWidget* button,
                              GdkDragContext* drag_context,
                              BookmarkBarGtk* bar);
  static void OnButtonDragGet(GtkWidget* widget, GdkDragContext* context,
                              GtkSelectionData* selection_data,
                              guint target_type, guint time,
                              BookmarkBarGtk* bar);

  // GtkButton callbacks for folder buttons.
  static void OnFolderClicked(GtkWidget* sender,
                              BookmarkBarGtk* bar);

  // GtkToolbar callbacks.
  static gboolean OnToolbarDragMotion(GtkToolbar* toolbar,
                                      GdkDragContext* context,
                                      gint x,
                                      gint y,
                                      guint time,
                                      BookmarkBarGtk* bar);
  static void OnToolbarDragLeave(GtkToolbar* toolbar,
                                 GdkDragContext* context,
                                 guint time,
                                 BookmarkBarGtk* bar);
  static void OnToolbarSizeAllocate(GtkWidget* widget,
                                    GtkAllocation* allocation,
                                    BookmarkBarGtk* bar);

  // Used for both folder buttons and the toolbar.
  static void OnDragReceived(GtkWidget* widget,
                             GdkDragContext* context,
                             gint x, gint y,
                             GtkSelectionData* selection_data,
                             guint target_type, guint time,
                             BookmarkBarGtk* bar);

  // GtkEventBox callbacks.
  static gboolean OnEventBoxExpose(GtkWidget* widget, GdkEventExpose* event,
                                   BookmarkBarGtk* bar);
  static void OnEventBoxDestroy(GtkWidget* widget, BookmarkBarGtk* bar);

  // GtkVSeparator callbacks.
  static gboolean OnSeparatorExpose(GtkWidget* widget, GdkEventExpose* event,
                                    BookmarkBarGtk* bar);

  // Callbacks on our parent widget.
  static void OnParentSizeAllocate(GtkWidget* widget,
                                   GtkAllocation* allocation,
                                   BookmarkBarGtk* bar);

  // |throbbing_widget_| callback.
  static void OnThrobbingWidgetDestroy(GtkWidget* widget,
                                       BookmarkBarGtk* bar);

  // ProfileSyncServiceObserver method.
  virtual void OnStateChanged();

  // Overriden from BookmarkBarInstructionsGtk::Delegate.
  virtual void ShowImportDialog();

  Profile* profile_;

  // Used for opening urls.
  PageNavigator* page_navigator_;

  Browser* browser_;
  BrowserWindowGtk* window_;

  // Provides us with the offset into the background theme image.
  TabstripOriginProvider* tabstrip_origin_provider_;

  // Model providing details as to the starred entries/groups that should be
  // shown. This is owned by the Profile.
  BookmarkModel* model_;

  // Contains |bookmark_hbox_|. Event box exists to prevent leakage of
  // background color from the toplevel application window's GDK window.
  OwnedWidgetGtk event_box_;

  // Used to float the bookmark bar when on the NTP.
  GtkWidget* ntp_padding_box_;

  // Used to paint the background of the bookmark bar when in floating mode.
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
  OwnedWidgetGtk bookmark_toolbar_;

  // The button that shows extra bookmarks that don't fit on the bookmark
  // bar.
  GtkWidget* overflow_button_;

  // The other bookmarks button.
  GtkWidget* other_bookmarks_button_;

  // The sync error button.
  GtkWidget* sync_error_button_;

  // A pointer to the ProfileSyncService instance if one exists.
  ProfileSyncService* sync_service_;

  // The BookmarkNode from the model being dragged. NULL when we aren't
  // dragging.
  const BookmarkNode* dragged_node_;

  // We create a GtkToolbarItem from |dragged_node_| ;or display.
  GtkToolItem* toolbar_drop_item_;

  // Theme provider for building buttons.
  GtkThemeProvider* theme_provider_;

  // Whether we should show the instructional text in the bookmark bar.
  bool show_instructions_;

  MenuBarHelper menu_bar_helper_;

  // The last displayed right click menu, or NULL if no menus have been
  // displayed yet.
  // The controller.
  scoped_ptr<BookmarkContextMenuGtk> current_context_menu_controller_;
  // The view.
  scoped_ptr<MenuGtk> current_context_menu_;

  // The last displayed left click menu, or NULL if no menus have been
  // displayed yet.
  scoped_ptr<BookmarkMenuController> current_menu_;

  scoped_ptr<SlideAnimation> slide_animation_;

  // Whether we are currently configured as floating (detached from the
  // toolbar). This reflects our actual state, and can be out of sync with
  // what ShouldShowFloating() returns.
  bool floating_;

  // Used to optimize out |bookmark_toolbar_| size-allocate events we don't
  // need to respond to.
  int last_allocation_width_;

  NotificationRegistrar registrar_;

  // The size of the tab contents last time we forced a paint. We keep track
  // of this so we don't force too many paints.
  gfx::Size last_tab_contents_size_;

  // The currently throbbing widget. This is NULL if no widget is throbbing.
  // We track it because we only want to allow one widget to throb at a time.
  GtkWidget* throbbing_widget_;

  ScopedRunnableMethodFactory<BookmarkBarGtk> method_factory_;
};

#endif  // CHROME_BROWSER_GTK_BOOKMARK_BAR_GTK_H_
