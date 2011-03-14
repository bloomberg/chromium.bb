// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_bubble_host.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/infobars/infobar_container.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"
#include "chrome/browser/ui/views/tabs/abstract_tab_strip_view.h"
#include "chrome/browser/ui/views/unhandled_keyboard_event_handler.h"
#include "content/common/notification_registrar.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/native_widget_types.h"
#include "views/controls/single_split_view.h"
#include "views/window/client_view.h"
#include "views/window/window_delegate.h"

#if defined(OS_WIN)
#include "chrome/browser/hang_monitor/hung_plugin_action.h"
#include "chrome/browser/hang_monitor/hung_window_detector.h"
#include "views/controls/menu/native_menu_win.h"
#endif

// NOTE: For more information about the objects and files in this directory,
// view: http://dev.chromium.org/developers/design-documents/browser-window

class AccessiblePaneView;
class BookmarkBarView;
class Browser;
class BrowserBubble;
class BrowserViewLayout;
class ContentsContainer;
class DownloadShelfView;
class EncodingMenuModel;
class FullscreenExitBubble;
class HtmlDialogUIDelegate;
class InfoBarContainer;
class LocationBarView;
class SideTabStrip;
class StatusBubbleViews;
class TabContentsContainer;
class TabStripModel;
class ToolbarView;
class ZoomMenuModel;
class Extension;

#if defined(OS_WIN)
class AeroPeekManager;
class JumpList;
#endif

namespace views {
class ExternalFocusTracker;
class Menu;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView
//
//  A ClientView subclass that provides the contents of a browser window,
//  including the TabStrip, toolbars, download shelves, the content area etc.
//
class BrowserView : public BrowserBubbleHost,
                    public BrowserWindow,
                    public BrowserWindowTesting,
                    public NotificationObserver,
                    public TabStripModelObserver,
                    public ui::SimpleMenuModel::Delegate,
                    public views::WindowDelegate,
                    public views::ClientView,
                    public InfoBarContainer::Delegate,
                    public views::SingleSplitView::Observer {
 public:
  // The browser view's class name.
  static const char kViewClassName[];

  explicit BrowserView(Browser* browser);
  virtual ~BrowserView();

  void set_frame(BrowserFrame* frame) { frame_ = frame; }
  BrowserFrame* frame() const { return frame_; }

  // Returns a pointer to the BrowserView* interface implementation (an
  // instance of this object, typically) for a given native window, or NULL if
  // there is no such association.
  static BrowserView* GetBrowserViewForNativeWindow(gfx::NativeWindow window);

  // Returns a Browser instance of this view.
  Browser* browser() const { return browser_.get(); }

  // Called by the frame to notify the BrowserView that it was moved, and that
  // any dependent popup windows should be repositioned.
  void WindowMoved();

  // Called by the frame to notify the BrowserView that a move or resize was
  // initiated.
  void WindowMoveOrResizeStarted();

  // Returns the apparent bounds of the toolbar, in BrowserView coordinates.
  // These differ from |toolbar_.bounds()| in that they match where the toolbar
  // background image is drawn -- slightly outside the "true" bounds
  // horizontally, and, when using vertical tabs, behind the tab column.
  gfx::Rect GetToolbarBounds() const;

  // Returns the bounds of the content area, in the coordinates of the
  // BrowserView's parent.
  gfx::Rect GetClientAreaBounds() const;

  // Returns the constraining bounding box that should be used to lay out the
  // FindBar within. This is _not_ the size of the find bar, just the bounding
  // box it should be laid out within. The coordinate system of the returned
  // rect is in the coordinate system of the frame, since the FindBar is a child
  // window.
  gfx::Rect GetFindBarBoundingBox() const;

  // Returns the preferred height of the TabStrip. Used to position the OTR
  // avatar icon.
  int GetTabStripHeight() const;

  // Takes some view's origin (relative to this BrowserView) and offsets it such
  // that it can be used as the source origin for seamlessly tiling the toolbar
  // background image over that view.
  gfx::Point OffsetPointForToolbarBackgroundImage(
      const gfx::Point& point) const;

  // Returns the width of the currently displayed sidebar or 0.
  int GetSidebarWidth() const;

  // Accessor for the TabStrip.
  AbstractTabStripView* tabstrip() const { return tabstrip_; }

  // Accessor for the Toolbar.
  ToolbarView* toolbar() const { return toolbar_; }

  // Returns true if various window components are visible.
  bool IsTabStripVisible() const;

  // Returns true if the vertical tabstrip is in use.
  bool UseVerticalTabs() const;

  // Returns true if the profile associated with this Browser window is
  // incognito.
  bool IsOffTheRecord() const;

  // Returns true if the non-client view should render the Off-The-Record
  // avatar icon if the window is incognito.
  bool ShouldShowOffTheRecordAvatar() const;

  // Handle the specified |accelerator| being pressed.
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  // Provides the containing frame with the accelerator for the specified
  // command id. This can be used to provide menu item shortcut hints etc.
  // Returns true if an accelerator was found for the specified |cmd_id|, false
  // otherwise.
  bool GetAccelerator(int cmd_id, ui::Accelerator* accelerator);

  // Shows the next app-modal dialog box, if there is one to be shown, or moves
  // an existing showing one to the front. Returns true if one was shown or
  // activated, false if none was shown.
  bool ActivateAppModalDialog() const;

  // Returns the selected TabContents[Wrapper]. Used by our NonClientView's
  // TabIconView::TabContentsProvider implementations.
  // TODO(beng): exposing this here is a bit bogus, since it's only used to
  // determine loading state. It'd be nicer if we could change this to be
  // bool IsSelectedTabLoading() const; or something like that. We could even
  // move it to a WindowDelegate subclass.
  TabContents* GetSelectedTabContents() const;
  TabContentsWrapper* GetSelectedTabContentsWrapper() const;

  // Retrieves the icon to use in the frame to indicate an OTR window.
  SkBitmap GetOTRAvatarIcon();

#if defined(OS_WIN)
  // Called right before displaying the system menu to allow the BrowserView
  // to add or delete entries.
  void PrepareToRunSystemMenu(HMENU menu);
#endif

  // Returns true if the Browser object associated with this BrowserView is a
  // normal-type window (i.e. a browser window, not an app or popup).
  bool IsBrowserTypeNormal() const {
    return browser_->type() == Browser::TYPE_NORMAL;
  }

  // Returns true if the Browser object associated with this BrowserView is a
  // app panel window.
  bool IsBrowserTypePanel() const {
    return browser_->type() == Browser::TYPE_APP_PANEL;
  }

  // Returns true if the Browser object associated with this BrowserView is a
  // popup window.
  bool IsBrowserTypePopup() const {
    return (browser_->type() & Browser::TYPE_POPUP) != 0;
  }

  // Register preferences specific to this view.
  static void RegisterBrowserViewPrefs(PrefService* prefs);

  // Returns true if the specified point(BrowserView coordinates) is in
  // in the window caption area of the browser window.
  bool IsPositionInWindowCaption(const gfx::Point& point);

  // Returns whether the fullscreen bubble is visible or not.
  virtual bool IsFullscreenBubbleVisible() const;

  // Invoked from the frame when the full screen state changes. This is only
  // used on Linux.
  void FullScreenStateChanged();

  // Restores the focused view. This is also used to set the initial focus
  // when a new browser window is created.
  void RestoreFocus();

  // Overridden from BrowserWindow:
  virtual void Show() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual void FlashFrame() OVERRIDE;
  virtual gfx::NativeWindow GetNativeHandle() OVERRIDE;
  virtual BrowserWindowTesting* GetBrowserWindowTesting() OVERRIDE;
  virtual StatusBubble* GetStatusBubble() OVERRIDE;
  virtual void SelectedTabToolbarSizeChanged(bool is_animating) OVERRIDE;
  virtual void UpdateTitleBar() OVERRIDE;
  virtual void ShelfVisibilityChanged() OVERRIDE;
  virtual void UpdateDevTools() OVERRIDE;
  virtual void UpdateLoadingAnimations(bool should_animate) OVERRIDE;
  virtual void SetStarredState(bool is_starred) OVERRIDE;
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual void SetFullscreen(bool fullscreen) OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual LocationBar* GetLocationBar() const OVERRIDE;
  virtual void SetFocusToLocationBar(bool select_all) OVERRIDE;
  virtual void UpdateReloadStopState(bool is_loading, bool force) OVERRIDE;
  virtual void UpdateToolbar(TabContentsWrapper* contents,
                             bool should_restore_state) OVERRIDE;
  virtual void FocusToolbar() OVERRIDE;
  virtual void FocusAppMenu() OVERRIDE;
  virtual void FocusBookmarksToolbar() OVERRIDE;
  virtual void FocusChromeOSStatus() OVERRIDE {}
  virtual void RotatePaneFocus(bool forwards) OVERRIDE;
  virtual void DestroyBrowser() OVERRIDE;
  virtual bool IsBookmarkBarVisible() const OVERRIDE;
  virtual bool IsBookmarkBarAnimating() const OVERRIDE;
  virtual bool IsTabStripEditable() const OVERRIDE;
  virtual bool IsToolbarVisible() const OVERRIDE;
  virtual void DisableInactiveFrame() OVERRIDE;
  virtual void ConfirmSetDefaultSearchProvider(
      TabContents* tab_contents,
      TemplateURL* template_url,
      TemplateURLModel* template_url_model) OVERRIDE;
  virtual void ConfirmAddSearchProvider(const TemplateURL* template_url,
                                        Profile* profile) OVERRIDE;
  virtual void ToggleBookmarkBar() OVERRIDE;
  virtual void ShowAboutChromeDialog() OVERRIDE;
  virtual void ShowUpdateChromeDialog() OVERRIDE;
  virtual void ShowTaskManager() OVERRIDE;
  virtual void ShowBackgroundPages() OVERRIDE;
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked)
      OVERRIDE;
  // TODO(beng): Not an override, move somewhere else.
  void SetDownloadShelfVisible(bool visible);
  virtual bool IsDownloadShelfVisible() const OVERRIDE;
  virtual DownloadShelf* GetDownloadShelf() OVERRIDE;
  virtual void ShowRepostFormWarningDialog(TabContents* tab_contents) OVERRIDE;
  virtual void ShowCollectedCookiesDialog(TabContents* tab_contents) OVERRIDE;
  virtual void ShowProfileErrorDialog(int message_id) OVERRIDE;
  virtual void ShowThemeInstallBubble() OVERRIDE;
  virtual void ConfirmBrowserCloseWithPendingDownloads() OVERRIDE;
  virtual void ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
                              gfx::NativeWindow parent_window) OVERRIDE;
  virtual void UserChangedTheme() OVERRIDE;
  virtual int GetExtraRenderViewHeight() const OVERRIDE;
  virtual void TabContentsFocused(TabContents* source) OVERRIDE;
  virtual void ShowPageInfo(Profile* profile,
                            const GURL& url,
                            const NavigationEntry::SSLStatus& ssl,
                            bool show_history) OVERRIDE;
  virtual void ShowAppMenu() OVERRIDE;
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event)
      OVERRIDE;
  virtual void ShowCreateWebAppShortcutsDialog(TabContents* tab_contents)
      OVERRIDE;
  virtual void ShowCreateChromeAppShortcutsDialog(
      Profile*, const Extension* app) OVERRIDE;
  virtual void Cut() OVERRIDE;
  virtual void Copy() OVERRIDE;
  virtual void Paste() OVERRIDE;
  virtual void ToggleTabStripMode() OVERRIDE;
  virtual void PrepareForInstant() OVERRIDE;
  virtual void ShowInstant(TabContents* preview_contents) OVERRIDE;
  virtual void HideInstant(bool instant_is_active) OVERRIDE;
  virtual gfx::Rect GetInstantBounds() OVERRIDE;
#if defined(OS_CHROMEOS)
  virtual void ShowKeyboardOverlay(gfx::NativeWindow owning_window) OVERRIDE;
#endif

  // Overridden from BrowserWindowTesting:
  virtual BookmarkBarView* GetBookmarkBarView() const OVERRIDE;
  virtual LocationBarView* GetLocationBarView() const OVERRIDE;
  virtual views::View* GetTabContentsContainerView() const OVERRIDE;
  virtual views::View* GetSidebarContainerView() const OVERRIDE;
  virtual ToolbarView* GetToolbarView() const OVERRIDE;

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Overridden from TabStripModelObserver:
  virtual void TabDetachedAt(TabContentsWrapper* contents, int index) OVERRIDE;
  virtual void TabDeselected(TabContentsWrapper* contents) OVERRIDE;
  virtual void TabSelectedAt(TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index,
                             bool user_gesture) OVERRIDE;
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int index) OVERRIDE;
  virtual void TabStripEmpty() OVERRIDE;

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id, ui::Accelerator* accelerator) OVERRIDE;
  virtual bool IsItemForCommandIdDynamic(int command_id) const OVERRIDE;
  virtual string16 GetLabelForCommandId(int command_id) const OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

  // Overridden from views::WindowDelegate:
  virtual bool CanResize() const OVERRIDE;
  virtual bool CanMaximize() const OVERRIDE;
  virtual bool CanActivate() const OVERRIDE;
  virtual bool IsModal() const OVERRIDE;
  virtual std::wstring GetWindowTitle() const OVERRIDE;
  virtual std::wstring GetAccessibleWindowTitle() const OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual bool ShouldShowWindowTitle() const OVERRIDE;
  virtual SkBitmap GetWindowAppIcon() OVERRIDE;
  virtual SkBitmap GetWindowIcon() OVERRIDE;
  virtual bool ShouldShowWindowIcon() const OVERRIDE;
  virtual bool ExecuteWindowsCommand(int command_id) OVERRIDE;
  virtual std::wstring GetWindowName() const OVERRIDE;
  virtual void SaveWindowPlacement(const gfx::Rect& bounds,
                                   bool maximized) OVERRIDE;
  virtual bool GetSavedWindowBounds(gfx::Rect* bounds) const OVERRIDE;
  virtual bool GetSavedMaximizedState(bool* maximized) const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::ClientView* CreateClientView(views::Window* window) OVERRIDE;
  virtual void OnWindowActivate(bool active) OVERRIDE;

  // Overridden from views::ClientView:
  virtual bool CanClose() OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual gfx::Size GetMinimumSize() OVERRIDE;

  // InfoBarContainer::Delegate overrides
  virtual void InfoBarContainerSizeChanged(bool is_animating) OVERRIDE;

  // views::SingleSplitView::Observer overrides:
  virtual bool SplitHandleMoved(views::SingleSplitView* view) OVERRIDE;

 protected:
  // Appends to |toolbars| a pointer to each AccessiblePaneView that
  // can be traversed using F6, in the order they should be traversed.
  // Abstracted here so that it can be extended for Chrome OS.
  virtual void GetAccessiblePanes(
      std::vector<AccessiblePaneView*>* panes);

  // Save the current focused view to view storage
  void SaveFocusedView();

  int last_focused_view_storage_id() const {
    return last_focused_view_storage_id_;
  }

  // Overridden from views::View:
  virtual std::string GetClassName() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // Factory Methods.
  // Returns a new LayoutManager for this browser view. A subclass may
  // override to implemnet different layout pocily.
  virtual views::LayoutManager* CreateLayoutManager() const;

  // Initializes a new TabStrip for the browser view. This can be performed
  // multiple times over the life of the browser, and is run when the display
  // mode for the tabstrip changes from horizontal to vertical.
  virtual void InitTabStrip(TabStripModel* tab_strip_model);

  // Browser window related initializations.
  virtual void Init();

 private:
  friend class BrowserViewLayout;
  FRIEND_TEST_ALL_PREFIXES(BrowserViewsAccessibilityTest,
                           TestAboutChromeViewAccObj);

#if defined(OS_WIN)
  // Creates the system menu.
  void InitSystemMenu();
#endif

  // Get the X value, in this BrowserView's coordinate system, where
  // the points of the infobar arrows should be anchored.  This is the
  // center of the omnibox location icon.
  int GetInfoBarArrowCenterX() const;

  // Returns the BrowserViewLayout.
  BrowserViewLayout* GetBrowserViewLayout() const;

  // Layout the Status Bubble.
  void LayoutStatusBubble();

  // Prepare to show the Bookmark Bar for the specified TabContents. Returns
  // true if the Bookmark Bar can be shown (i.e. it's supported for this
  // Browser type) and there should be a subsequent re-layout to show it.
  // |contents| can be NULL.
  bool MaybeShowBookmarkBar(TabContentsWrapper* contents);

  // Prepare to show an Info Bar for the specified TabContents. Returns true
  // if there is an Info Bar to show and one is supported for this Browser
  // type, and there should be a subsequent re-layout to show it.
  // |contents| can be NULL.
  bool MaybeShowInfoBar(TabContentsWrapper* contents);

  // Updates sidebar UI according to the current tab and sidebar state.
  void UpdateSidebar();
  // Displays active sidebar linked to the |tab_contents| or hides sidebar UI,
  // if there's no such sidebar.
  void UpdateSidebarForContents(TabContentsWrapper* tab_contents);

  // Updated devtools window for given contents.
  void UpdateDevToolsForContents(TabContentsWrapper* tab_contents);

  // Updates various optional child Views, e.g. Bookmarks Bar, Info Bar or the
  // Download Shelf in response to a change notification from the specified
  // |contents|. |contents| can be NULL. In this case, all optional UI will be
  // removed.
  void UpdateUIForContents(TabContentsWrapper* contents);

  // Updates an optional child View, e.g. Bookmarks Bar, Info Bar, Download
  // Shelf. If |*old_view| differs from new_view, the old_view is removed and
  // the new_view is added. This is intended to be used when swapping in/out
  // child views that are referenced via a field.
  // Returns true if anything was changed, and a re-Layout is now required.
  bool UpdateChildViewAndLayout(views::View* new_view, views::View** old_view);

  // Invoked to update the necessary things when our fullscreen state changes
  // to |fullscreen|. On Windows this is invoked immediately when we toggle the
  // full screen state. On Linux changing the fullscreen state is async, so we
  // ask the window to change it's fullscreen state, then when we get
  // notification that it succeeded this method is invoked.
  void ProcessFullscreen(bool fullscreen);

  // Copy the accelerator table from the app resources into something we can
  // use.
  void LoadAccelerators();

#if defined(OS_WIN)
  // Builds the correct menu for when we have minimal chrome.
  void BuildSystemMenuForBrowserWindow();
  void BuildSystemMenuForAppOrPopupWindow(bool is_app);
#endif

  // Retrieves the command id for the specified Windows app command.
  int GetCommandIDForAppCommandID(int app_command_id) const;

  // Callback for the loading animation(s) associated with this view.
  void LoadingAnimationCallback();

  // Initialize the hung plugin detector.
  void InitHangMonitor();

  // Possibly records a user metrics action corresponding to the passed-in
  // accelerator.  Only implemented for Chrome OS, where we're interested in
  // learning about how frequently the top-row keys are used.
  void UpdateAcceleratorMetrics(const views::Accelerator& accelerator,
                                int command_id);

  // Invoked from TabSelectedAt or when instant is made active.  Is
  // |change_tab_contents| is true, |new_contents| is added to the view
  // hierarchy, if |change_tab_contents| is false, it's assumed |new_contents|
  // has already been added to the view hierarchy.
  void ProcessTabSelected(TabContentsWrapper* new_contents,
                          bool change_tab_contents);

  // Exposes resize corner size to BrowserViewLayout.
  gfx::Size GetResizeCornerSize() const;

  // Shows the about chrome modal dialog and returns the Window object.
  views::Window* DoShowAboutChromeDialog();

  // Last focused view that issued a tab traversal.
  int last_focused_view_storage_id_;

  // The BrowserFrame that hosts this view.
  BrowserFrame* frame_;

  // The Browser object we are associated with.
  scoped_ptr<Browser> browser_;

  // BrowserView layout (LTR one is pictured here).
  //
  // --------------------------------------------------------------------------
  // |         | Tabs (1)                                                     |
  // |         |--------------------------------------------------------------|
  // |         | Navigation buttons, menus and the address bar (toolbar_)     |
  // |         |--------------------------------------------------------------|
  // |         | All infobars (infobar_container_) *                          |
  // |         |--------------------------------------------------------------|
  // |         | Bookmarks (bookmark_bar_view_) *                             |
  // |         |--------------------------------------------------------------|
  // |         |Page content (contents_)              ||                      |
  // |         |--------------------------------------|| Sidebar content      |
  // |         || contents_container_ and/or         ||| (sidebar_container_) |
  // |         || preview_container_                 |||                      |
  // |         ||                                    |(3)                     |
  // | Tabs (2)||                                    |||                      |
  // |         ||                                    |||                      |
  // |         ||                                    |||                      |
  // |         ||                                    |||                      |
  // |         |--------------------------------------||                      |
  // |         |==(4)=========================================================|
  // |         |                                                              |
  // |         |                                                              |
  // |         | Debugger (devtools_container_)                               |
  // |         |                                                              |
  // |         |                                                              |
  // |         |--------------------------------------------------------------|
  // |         | Active downloads (download_shelf_)                           |
  // --------------------------------------------------------------------------
  //
  // (1) - tabstrip_, default position
  // (2) - tabstrip_, position when side tabs are enabled
  // (3) - sidebar_split_
  // (4) - contents_split_
  //
  // * - The bookmark bar and info bar are swapped when on the new tab page.
  //     Additionally contents_ is positioned on top of the bookmark bar when
  //     the bookmark bar is detached. This is done to allow the
  //     preview_container_ to appear over the bookmark bar.

  // Tool/Info bars that we are currently showing. Used for layout.
  // active_bookmark_bar_ is either NULL, if the bookmark bar isn't showing,
  // or is bookmark_bar_view_ if the bookmark bar is showing.
  views::View* active_bookmark_bar_;

  // The TabStrip.
  AbstractTabStripView* tabstrip_;

  // The Toolbar containing the navigation buttons, menus and the address bar.
  ToolbarView* toolbar_;

  // The Bookmark Bar View for this window. Lazily created.
  scoped_ptr<BookmarkBarView> bookmark_bar_view_;

  // The download shelf view (view at the bottom of the page).
  scoped_ptr<DownloadShelfView> download_shelf_;

  // The InfoBarContainer that contains InfoBars for the current tab.
  InfoBarContainer* infobar_container_;

  // The view that contains sidebar for the current tab.
  TabContentsContainer* sidebar_container_;

  // Split view containing the contents container and sidebar container.
  views::SingleSplitView* sidebar_split_;

  // The view that contains the selected TabContents.
  TabContentsContainer* contents_container_;

  // The view that contains devtools window for the selected TabContents.
  TabContentsContainer* devtools_container_;

  // The view that contains instant's TabContents.
  TabContentsContainer* preview_container_;

  // The view managing both the contents_container_ and preview_container_.
  ContentsContainer* contents_;

  // Split view containing the contents container and devtools container.
  views::SingleSplitView* contents_split_;

  // Tracks and stores the last focused view which is not the
  // devtools_container_ or any of its children. Used to restore focus once
  // the devtools_container_ is hidden.
  scoped_ptr<views::ExternalFocusTracker> devtools_focus_tracker_;

  // The Status information bubble that appears at the bottom of the window.
  scoped_ptr<StatusBubbleViews> status_bubble_;

  // A mapping between accelerators and commands.
  std::map<views::Accelerator, int> accelerator_table_;

  // True if we have already been initialized.
  bool initialized_;

  // True if we should ignore requests to layout.  This is set while toggling
  // fullscreen mode on and off to reduce jankiness.
  bool ignore_layout_;

  scoped_ptr<FullscreenExitBubble> fullscreen_bubble_;

#if defined(OS_WIN)
  // The additional items we insert into the system menu.
  scoped_ptr<views::SystemMenuModel> system_menu_contents_;
  scoped_ptr<ZoomMenuModel> zoom_menu_contents_;
  scoped_ptr<EncodingMenuModel> encoding_menu_contents_;
  // The wrapped system menu itself.
  scoped_ptr<views::NativeMenuWin> system_menu_;

  // This object is used to perform periodic actions in a worker
  // thread. It is currently used to monitor hung plugin windows.
  WorkerThreadTicker ticker_;

  // This object is initialized with the frame window HWND. This
  // object is also passed as a tick handler with the ticker_ object.
  // It is used to periodically monitor for hung plugin windows
  HungWindowDetector hung_window_detector_;

  // This object is invoked by hung_window_detector_ when it detects a hung
  // plugin window.
  HungPluginAction hung_plugin_action_;

  // The custom JumpList for Windows 7.
  scoped_ptr<JumpList> jumplist_;

  // The custom AeroPeek manager for Windows 7.
  scoped_ptr<AeroPeekManager> aeropeek_manager_;
#endif

  // The timer used to update frames for the Loading Animation.
  base::RepeatingTimer<BrowserView> loading_animation_timer_;

  UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  NotificationRegistrar registrar_;

  // Used to measure the loading spinner animation rate.
  base::TimeTicks last_animation_time_;

  DISALLOW_COPY_AND_ASSIGN(BrowserView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_H_
