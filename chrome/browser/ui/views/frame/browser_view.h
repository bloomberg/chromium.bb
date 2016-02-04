// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/infobar_container_delegate.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views_context.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/web_contents_close_handler.h"
#include "chrome/browser/ui/views/load_complete_listener.h"
#include "components/omnibox/browser/omnibox_popup_model_observer.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/client_view.h"

#if defined(OS_WIN)
#include "chrome/browser/hang_monitor/hung_plugin_action.h"
#include "chrome/browser/hang_monitor/hung_window_detector.h"
#endif

// NOTE: For more information about the objects and files in this directory,
// view: http://dev.chromium.org/developers/design-documents/browser-window

class BookmarkBarView;
class Browser;
class BrowserViewLayout;
class ContentsLayoutManager;
class DownloadShelfView;
class ExclusiveAccessBubbleViews;
class InfoBarContainerView;
class LocationBarView;
class StatusBubbleViews;
class TabStrip;
class ToolbarView;
class TopContainerView;
class WebContentsCloseHandler;

#if defined(OS_WIN)
class JumpList;
#endif

namespace extensions {
class Extension;
}

namespace views {
class AccessiblePaneView;
class ExternalFocusTracker;
class WebView;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserView
//
//  A ClientView subclass that provides the contents of a browser window,
//  including the TabStrip, toolbars, download shelves, the content area etc.
//
class BrowserView : public BrowserWindow,
                    public TabStripModelObserver,
                    public ui::AcceleratorProvider,
                    public views::WidgetDelegate,
                    public views::WidgetObserver,
                    public views::ClientView,
                    public InfoBarContainerDelegate,
                    public LoadCompleteListener::Delegate,
                    public OmniboxPopupModelObserver,
                    public ExclusiveAccessContext,
                    public ExclusiveAccessBubbleViewsContext {
 public:
  // The browser view's class name.
  static const char kViewClassName[];

  BrowserView();
  ~BrowserView() override;

  // Takes ownership of |browser|.
  void Init(Browser* browser);

  void set_frame(BrowserFrame* frame) { frame_ = frame; }
  BrowserFrame* frame() const { return frame_; }

  // Returns a pointer to the BrowserView* interface implementation (an
  // instance of this object, typically) for a given native window, or null if
  // there is no such association.
  //
  // Don't use this unless you only have a NativeWindow. In nearly all
  // situations plumb through browser and use it.
  static BrowserView* GetBrowserViewForNativeWindow(gfx::NativeWindow window);

  // Returns the BrowserView used for the specified Browser.
  static BrowserView* GetBrowserViewForBrowser(const Browser* browser);

  // Paints a 1 device-pixel-thick horizontal line (regardless of device scale
  // factor) at either the very bottom or very top of the interior of |bounds|,
  // depending on |at_bottom|.
  static void Paint1pxHorizontalLine(gfx::Canvas* canvas,
                                     SkColor color,
                                     const gfx::Rect& bounds,
                                     bool at_bottom);

  // Returns a Browser instance of this view.
  Browser* browser() { return browser_.get(); }
  const Browser* browser() const { return browser_.get(); }

  // Initializes (or re-initializes) the status bubble.  We try to only create
  // the bubble once and re-use it for the life of the browser, but certain
  // events (such as changing enabling/disabling Aero on Win) can force a need
  // to change some of the bubble's creation parameters.
  void InitStatusBubble();

  // Returns the apparent bounds of the toolbar, in BrowserView coordinates.
  // These differ from |toolbar_.bounds()| in that they match where the toolbar
  // background image is drawn -- slightly outside the "true" bounds
  // horizontally. Note that this returns the bounds for the toolbar area.
  gfx::Rect GetToolbarBounds() const;

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

  // Container for the tabstrip, toolbar, etc.
  TopContainerView* top_container() { return top_container_; }

  // Accessor for the TabStrip.
  TabStrip* tabstrip() { return tabstrip_; }

  // Accessor for the Toolbar.
  ToolbarView* toolbar() { return toolbar_; }

  // Bookmark bar may be null, for example for pop-ups.
  BookmarkBarView* bookmark_bar() { return bookmark_bar_view_.get(); }

  // Returns the do-nothing view which controls the z-order of the find bar
  // widget relative to views which paint into layers and views which have an
  // associated NativeView. The presence / visibility of this view is not
  // indicative of the visibility of the find bar widget or even whether
  // FindBarController is initialized.
  View* find_bar_host_view() { return find_bar_host_view_; }

  // Accessor for the InfobarContainer.
  InfoBarContainerView* infobar_container() { return infobar_container_; }

  // Accessor for the FullscreenExitBubbleViews.
  ExclusiveAccessBubbleViews* exclusive_access_bubble() {
    return exclusive_access_bubble_.get();
  }

  // Returns true if various window components are visible.
  bool IsTabStripVisible() const;

  // Returns true if the profile associated with this Browser window is
  // incognito.
  bool IsOffTheRecord() const;

  // Returns true if the profile associated with this Browser window is
  // a guest session.
  bool IsGuestSession() const;

  // Returns true if the profile associated with this Browser window is
  // not off the record or a guest session.
  bool IsRegularOrGuestSession() const;

  // Returns true if the non-client view should render an avatar icon.
  bool ShouldShowAvatar() const;

  // Provides the containing frame with the accelerator for the specified
  // command id. This can be used to provide menu item shortcut hints etc.
  // Returns true if an accelerator was found for the specified |cmd_id|, false
  // otherwise.
  bool GetAccelerator(int cmd_id, ui::Accelerator* accelerator) const;

  // Returns true if the specificed |accelerator| is registered with this view.
  bool IsAcceleratorRegistered(const ui::Accelerator& accelerator);

  // Returns the active WebContents. Used by our NonClientView's
  // TabIconView::TabContentsProvider implementations.
  // TODO(beng): exposing this here is a bit bogus, since it's only used to
  // determine loading state. It'd be nicer if we could change this to be
  // bool IsSelectedTabLoading() const; or something like that. We could even
  // move it to a WindowDelegate subclass.
  content::WebContents* GetActiveWebContents() const;

  // Retrieves the icon to use in the frame to indicate an OTR window.
  gfx::ImageSkia GetOTRAvatarIcon() const;

  // Returns true if the Browser object associated with this BrowserView is a
  // tabbed-type window (i.e. a browser window, not an app or popup).
  bool IsBrowserTypeNormal() const {
    return browser_->is_type_tabbed();
  }

  // See ImmersiveModeController for description.
  ImmersiveModeController* immersive_mode_controller() const {
    return immersive_mode_controller_.get();
  }

  // Returns true if the view has been initialized.
  bool initialized() const { return initialized_; }

  // Restores the focused view. This is also used to set the initial focus
  // when a new browser window is created.
  void RestoreFocus();

  // Called after the widget's fullscreen state is changed without going through
  // FullscreenController. This method does any processing which was skipped.
  // Only exiting fullscreen in this way is currently supported.
  void FullscreenStateChanged();

  // Overridden from BrowserWindow:
  void Show() override;
  void ShowInactive() override;
  void Hide() override;
  void SetBounds(const gfx::Rect& bounds) override;
  void Close() override;
  void Activate() override;
  void Deactivate() override;
  bool IsActive() const override;
  void FlashFrame(bool flash) override;
  bool IsAlwaysOnTop() const override;
  void SetAlwaysOnTop(bool always_on_top) override;
  gfx::NativeWindow GetNativeWindow() const override;
  StatusBubble* GetStatusBubble() override;
  void UpdateTitleBar() override;
  void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) override;
  void UpdateDevTools() override;
  void UpdateLoadingAnimations(bool should_animate) override;
  void SetStarredState(bool is_starred) override;
  void SetTranslateIconToggled(bool is_lit) override;
  void OnActiveTabChanged(content::WebContents* old_contents,
                          content::WebContents* new_contents,
                          int index,
                          int reason) override;
  void ZoomChangedForActiveTab(bool can_show_bubble) override;
  gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  gfx::Rect GetBounds() const override;
  gfx::Size GetContentsSize() const override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void EnterFullscreen(const GURL& url,
                       ExclusiveAccessBubbleType bubble_type,
                       bool with_toolbar) override;
  void ExitFullscreen() override;
  void UpdateExclusiveAccessExitBubbleContent(
      const GURL& url,
      ExclusiveAccessBubbleType bubble_type) override;
  void OnExclusiveAccessUserInput() override;
  bool ShouldHideUIForFullscreen() const override;
  bool IsFullscreen() const override;
  bool IsFullscreenBubbleVisible() const override;
#if defined(OS_WIN)
  void SetMetroSnapMode(bool enable) override;
  bool IsInMetroSnapMode() const override;
#endif  // defined(OS_WIN)
  LocationBar* GetLocationBar() const override;
  void SetFocusToLocationBar(bool select_all) override;
  void UpdateReloadStopState(bool is_loading, bool force) override;
  void UpdateToolbar(content::WebContents* contents) override;
  void ResetToolbarTabState(content::WebContents* contents) override;
  void FocusToolbar() override;
  ToolbarActionsBar* GetToolbarActionsBar() override;
  void ToolbarSizeChanged(bool is_animating) override;
  void FocusAppMenu() override;
  void FocusBookmarksToolbar() override;
  void FocusInfobars() override;
  void RotatePaneFocus(bool forwards) override;
  void DestroyBrowser() override;
  bool IsBookmarkBarVisible() const override;
  bool IsBookmarkBarAnimating() const override;
  bool IsTabStripEditable() const override;
  bool IsToolbarVisible() const override;
  gfx::Rect GetRootWindowResizerRect() const override;
  void ConfirmAddSearchProvider(TemplateURL* template_url,
                                Profile* profile) override;
  void ShowUpdateChromeDialog() override;
  void ShowBookmarkBubble(const GURL& url, bool already_bookmarked) override;
  void ShowBookmarkAppBubble(
      const WebApplicationInfo& web_app_info,
      const ShowBookmarkAppBubbleCallback& callback) override;
  autofill::SaveCardBubbleView* ShowSaveCreditCardBubble(
      content::WebContents* contents,
      autofill::SaveCardBubbleController* controller,
      bool is_user_gesture) override;
  void ShowTranslateBubble(content::WebContents* contents,
                           translate::TranslateStep step,
                           translate::TranslateErrors::Type error_type,
                           bool is_user_gesture) override;
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  void ShowOneClickSigninBubble(
      OneClickSigninBubbleType type,
      const base::string16& email,
      const base::string16& error_message,
      const StartSyncCallback& start_sync_callback) override;
#endif
  // TODO(beng): Not an override, move somewhere else.
  void SetDownloadShelfVisible(bool visible);
  bool IsDownloadShelfVisible() const override;
  DownloadShelf* GetDownloadShelf() override;
  void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadClosePreventionType dialog_type,
      bool app_modal,
      const base::Callback<void(bool)>& callback) override;
  void UserChangedTheme() override;
  void ShowWebsiteSettings(
      Profile* profile,
      content::WebContents* web_contents,
      const GURL& url,
      const security_state::SecurityStateModel::SecurityInfo& security_info)
      override;
  void ShowAppMenu() override;
  bool PreHandleKeyboardEvent(const content::NativeWebKeyboardEvent& event,
                              bool* is_keyboard_shortcut) override;
  void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  void CutCopyPaste(int command_id) override;
  WindowOpenDisposition GetDispositionForPopupBounds(
      const gfx::Rect& bounds) override;
  FindBar* CreateFindBar() override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;
  void ShowAvatarBubbleFromAvatarButton(
      AvatarBubbleMode mode,
      const signin::ManageAccountsParams& manage_accounts_params,
      signin_metrics::AccessPoint access_point) override;
  int GetRenderViewHeightInsetWithDetachedBookmarkBar() override;
  void ExecuteExtensionCommand(const extensions::Extension* extension,
                               const extensions::Command& command) override;
  ExclusiveAccessContext* GetExclusiveAccessContext() override;

  BookmarkBarView* GetBookmarkBarView() const;
  LocationBarView* GetLocationBarView() const;
  views::View* GetTabContentsContainerView() const;
  ToolbarView* GetToolbarView() const;

  // Overridden from TabStripModelObserver:
  void TabInsertedAt(content::WebContents* contents,
                     int index,
                     bool foreground) override;
  void TabDetachedAt(content::WebContents* contents, int index) override;
  void TabDeactivated(content::WebContents* contents) override;
  void TabStripEmpty() override;
  void WillCloseAllTabs() override;
  void CloseAllTabsCanceled() override;

  // Overridden from ui::AcceleratorProvider:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) override;

  // Overridden from views::WidgetDelegate:
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool CanActivate() const override;
  base::string16 GetWindowTitle() const override;
  base::string16 GetAccessibleWindowTitle() const override;
  views::View* GetInitiallyFocusedView() override;
  bool ShouldShowWindowTitle() const override;
  gfx::ImageSkia GetWindowAppIcon() override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;
  bool ExecuteWindowsCommand(int command_id) override;
  std::string GetWindowName() const override;
  void SaveWindowPlacement(const gfx::Rect& bounds,
                           ui::WindowShowState show_state) override;
  bool GetSavedWindowPlacement(const views::Widget* widget,
                               gfx::Rect* bounds,
                               ui::WindowShowState* show_state) const override;
  views::View* GetContentsView() override;
  views::ClientView* CreateClientView(views::Widget* widget) override;
  void OnWindowBeginUserBoundsChange() override;
  void OnWidgetMove() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  void GetAccessiblePanes(std::vector<View*>* panes) override;

  // Overridden from views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // Overridden from views::ClientView:
  bool CanClose() override;
  int NonClientHitTest(const gfx::Point& point) override;
  gfx::Size GetMinimumSize() const override;

  // InfoBarContainerDelegate:
  SkColor GetInfoBarSeparatorColor() const override;
  void InfoBarContainerStateChanged(bool is_animating) override;
  bool DrawInfoBarArrows(int* x) const override;

  // Overridden from views::View:
  const char* GetClassName() const override;
  void Layout() override;
  void PaintChildren(const ui::PaintContext& context) override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void ChildPreferredSizeChanged(View* child) override;
  void GetAccessibleState(ui::AXViewState* state) override;
  void OnThemeChanged() override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

  // Overridden from ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // OmniboxPopupModelObserver overrides
  void OnOmniboxPopupShownOrHidden() override;

  // ExclusiveAccessContext overrides
  Profile* GetProfile() override;
  content::WebContents* GetActiveWebContents() override;
  void HideDownloadShelf() override;
  void UnhideDownloadShelf() override;

  // ExclusiveAccessBubbleViewsContext overrides
  ExclusiveAccessManager* GetExclusiveAccessManager() override;
  views::Widget* GetBubbleAssociatedWidget() override;
  ui::AcceleratorProvider* GetAcceleratorProvider() override;
  gfx::NativeView GetBubbleParentView() const override;
  gfx::Point GetCursorPointInParent() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  bool IsImmersiveModeEnabled() override;
  gfx::Rect GetTopContainerBoundsInScreen() override;

  // Testing interface:
  views::View* GetContentsContainerForTest() { return contents_container_; }
  views::WebView* GetContentsWebViewForTest() { return contents_web_view_; }
  views::WebView* GetDevToolsWebViewForTest() { return devtools_web_view_; }

 private:
  // Do not friend BrowserViewLayout. Use the BrowserViewLayoutDelegate
  // interface to keep these two classes decoupled and testable.
  friend class BrowserViewLayoutDelegateImpl;
  FRIEND_TEST_ALL_PREFIXES(BrowserViewTest, BrowserView);

  enum FullscreenMode {
    NORMAL_FULLSCREEN,
    METRO_SNAP_FULLSCREEN
  };

  // Appends to |toolbars| a pointer to each AccessiblePaneView that
  // can be traversed using F6, in the order they should be traversed.
  void GetAccessiblePanes(std::vector<views::AccessiblePaneView*>* panes);

  // Constructs and initializes the child views.
  void InitViews();

  // Callback for the loading animation(s) associated with this view.
  void LoadingAnimationCallback();

  // LoadCompleteListener::Delegate implementation. Creates and initializes the
  // |jumplist_| after the first page load.
  void OnLoadCompleted() override;

  // Returns the BrowserViewLayout.
  BrowserViewLayout* GetBrowserViewLayout() const;

  // Returns the ContentsLayoutManager.
  ContentsLayoutManager* GetContentsLayoutManager() const;

  // Prepare to show the Bookmark Bar for the specified WebContents.
  // Returns true if the Bookmark Bar can be shown (i.e. it's supported for this
  // Browser type) and there should be a subsequent re-layout to show it.
  // |contents| can be null.
  bool MaybeShowBookmarkBar(content::WebContents* contents);

  // Moves the bookmark bar view to the specified parent, which may be null,
  // |this|, or |top_container_|. Ensures that |top_container_| stays in front
  // of |bookmark_bar_view_|.
  void SetBookmarkBarParent(views::View* new_parent);

  // Prepare to show an Info Bar for the specified WebContents. Returns
  // true if there is an Info Bar to show and one is supported for this Browser
  // type, and there should be a subsequent re-layout to show it.
  // |contents| can be null.
  bool MaybeShowInfoBar(content::WebContents* contents);

  // Updates devtools window for given contents. This method will show docked
  // devtools window for inspected |web_contents| that has docked devtools
  // and hide it for null or not inspected |web_contents|. It will also make
  // sure devtools window size and position are restored for given tab.
  // This method will not update actual DevTools WebContents, if not
  // |update_devtools_web_contents|. In this case, manual update is required.
  void UpdateDevToolsForContents(content::WebContents* web_contents,
                                 bool update_devtools_web_contents);

  // Updates various optional child Views, e.g. Bookmarks Bar, Info Bar or the
  // Download Shelf in response to a change notification from the specified
  // |contents|. |contents| can be null. In this case, all optional UI will be
  // removed.
  void UpdateUIForContents(content::WebContents* contents);

  // Invoked to update the necessary things when our fullscreen state changes
  // to |fullscreen|. On Windows this is invoked immediately when we toggle the
  // full screen state. On Linux changing the fullscreen state is async, so we
  // ask the window to change its fullscreen state, then when we get
  // notification that it succeeded this method is invoked.
  // If |url| is not empty, it is the URL of the page that requested fullscreen
  // (via the fullscreen JS API).
  // |bubble_type| determines what should be shown in the fullscreen exit
  // bubble.
  void ProcessFullscreen(bool fullscreen,
                         FullscreenMode mode,
                         const GURL& url,
                         ExclusiveAccessBubbleType bubble_type);

  // Returns whether immmersive fullscreen should replace fullscreen. This
  // should only occur for "browser-fullscreen" for tabbed-typed windows (not
  // for tab-fullscreen and not for app/popup type windows).
  bool ShouldUseImmersiveFullscreenForUrl(const GURL& url) const;

  // Copy the accelerator table from the app resources into something we can
  // use.
  void LoadAccelerators();

  // Retrieves the command id for the specified Windows app command.
  int GetCommandIDForAppCommandID(int app_command_id) const;

  // Initialize the hung plugin detector.
  void InitHangMonitor();

  // Possibly records a user metrics action corresponding to the passed-in
  // accelerator.  Only implemented for Chrome OS, where we're interested in
  // learning about how frequently the top-row keys are used.
  void UpdateAcceleratorMetrics(const ui::Accelerator& accelerator,
                                int command_id);

  // Calls |method| which is either WebContents::Cut, ::Copy, or ::Paste on
  // the given WebContents, returning true if it consumed the event.
  bool DoCutCopyPasteForWebContents(
      content::WebContents* contents,
      void (content::WebContents::*method)());

  // Shows the next app-modal dialog box, if there is one to be shown, or moves
  // an existing showing one to the front.
  void ActivateAppModalDialog() const;

  // Returns the max top arrow height for infobar.
  int GetMaxTopInfoBarArrowHeight();

  // Last focused view that issued a tab traversal.
  int last_focused_view_storage_id_;

  // The BrowserFrame that hosts this view.
  BrowserFrame* frame_;

  // The Browser object we are associated with.
  scoped_ptr<Browser> browser_;

  // BrowserView layout (LTR one is pictured here).
  //
  // --------------------------------------------------------------------
  // | TopContainerView (top_container_)                                |
  // |  --------------------------------------------------------------  |
  // |  | Tabs (tabstrip_)                                           |  |
  // |  |------------------------------------------------------------|  |
  // |  | Navigation buttons, address bar, menu (toolbar_)           |  |
  // |  --------------------------------------------------------------  |
  // |------------------------------------------------------------------|
  // | All infobars (infobar_container_) [1]                            |
  // |------------------------------------------------------------------|
  // | Bookmarks (bookmark_bar_view_) [1]                               |
  // |------------------------------------------------------------------|
  // | Contents container (contents_container_)                         |
  // |  --------------------------------------------------------------  |
  // |  |  devtools_web_view_                                        |  |
  // |  |------------------------------------------------------------|  |
  // |  |  contents_web_view_                                        |  |
  // |  --------------------------------------------------------------  |
  // |------------------------------------------------------------------|
  // | Active downloads (download_shelf_)                               |
  // --------------------------------------------------------------------
  //
  // [1] The bookmark bar and info bar are swapped when on the new tab page.
  //     Additionally when the bookmark bar is detached, contents_container_ is
  //     positioned on top of the bar while the tab's contents are placed below
  //     the bar.  This allows the find bar to always align with the top of
  //     contents_container_ regardless if there's bookmark or info bars.

  // The view that manages the tab strip, toolbar, and sometimes the bookmark
  // bar. Stacked top in the view hiearachy so it can be used to slide out
  // the top views in immersive fullscreen.
  TopContainerView* top_container_;

  // The TabStrip.
  TabStrip* tabstrip_;

  // The Toolbar containing the navigation buttons, menus and the address bar.
  ToolbarView* toolbar_;

  // The Bookmark Bar View for this window. Lazily created. May be null for
  // non-tabbed browsers like popups. May not be visible.
  scoped_ptr<BookmarkBarView> bookmark_bar_view_;

  // The do-nothing view which controls the z-order of the find bar widget
  // relative to views which paint into layers and views with an associated
  // NativeView.
  View* find_bar_host_view_;

  // The download shelf view (view at the bottom of the page).
  scoped_ptr<DownloadShelfView> download_shelf_;

  // The InfoBarContainerView that contains InfoBars for the current tab.
  InfoBarContainerView* infobar_container_;

  // The view that contains the selected WebContents.
  ContentsWebView* contents_web_view_;

  // The view that contains devtools window for the selected WebContents.
  views::WebView* devtools_web_view_;

  // The view managing the devtools and contents positions.
  // Handled by ContentsLayoutManager.
  views::View* contents_container_;

  // Tracks and stores the last focused view which is not the
  // devtools_web_view_ or any of its children. Used to restore focus once
  // the devtools_web_view_ is hidden.
  scoped_ptr<views::ExternalFocusTracker> devtools_focus_tracker_;

  // The Status information bubble that appears at the bottom of the window.
  scoped_ptr<StatusBubbleViews> status_bubble_;

  // A mapping between accelerators and commands.
  std::map<ui::Accelerator, int> accelerator_table_;

  // True if we have already been initialized.
  bool initialized_;

  // True if we're currently handling a theme change (i.e. inside
  // OnThemeChanged()).
  bool handling_theme_changed_;

  // True when in ProcessFullscreen(). The flag is used to avoid reentrance and
  // to ignore requests to layout while in ProcessFullscreen() to reduce
  // jankiness.
  bool in_process_fullscreen_;

  scoped_ptr<ExclusiveAccessBubbleViews> exclusive_access_bubble_;

#if defined(OS_WIN)
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

  // Helper class to listen for completion of first page load.
  scoped_ptr<LoadCompleteListener> load_complete_listener_;

  // The custom JumpList for Windows 7.
  scoped_refptr<JumpList> jumplist_;
#endif

  // The timer used to update frames for the Loading Animation.
  base::RepeatingTimer loading_animation_timer_;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Used to measure the loading spinner animation rate.
  base::TimeTicks last_animation_time_;

  // If this flag is set then SetFocusToLocationBar() will set focus to the
  // location bar even if the browser window is not active.
  bool force_location_bar_focus_;

  scoped_ptr<ImmersiveModeController> immersive_mode_controller_;

  scoped_ptr<WebContentsCloseHandler> web_contents_close_handler_;

  mutable base::WeakPtrFactory<BrowserView> activate_modal_dialog_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_H_
