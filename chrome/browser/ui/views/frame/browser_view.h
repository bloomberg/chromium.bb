// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/signin/signin_header_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window_testing_views.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/scroll_end_effect_controller.h"
#include "chrome/browser/ui/views/frame/web_contents_close_handler.h"
#include "chrome/browser/ui/views/load_complete_listener.h"
#include "components/infobars/core/infobar_container.h"
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
class FullscreenExitBubbleViews;
class InfoBarContainerView;
class LocationBarView;
class PermissionBubbleViewViews;
class StatusBubbleViews;
class SearchViewController;
class TabStrip;
class TabStripModel;
class ToolbarView;
class TopContainerView;
class WebContentsCloseHandler;

#if defined(OS_WIN)
class JumpList;
#endif

namespace autofill {
class PasswordGenerator;
}

namespace content {
class RenderFrameHost;
}

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
                    public BrowserWindowTesting,
                    public TabStripModelObserver,
                    public ui::AcceleratorProvider,
                    public views::WidgetDelegate,
                    public views::WidgetObserver,
                    public views::ClientView,
                    public infobars::InfoBarContainer::Delegate,
                    public LoadCompleteListener::Delegate,
                    public OmniboxPopupModelObserver {
 public:
  // The browser view's class name.
  static const char kViewClassName[];

  BrowserView();
  virtual ~BrowserView();

  // Takes ownership of |browser|.
  void Init(Browser* browser);

  void set_frame(BrowserFrame* frame) { frame_ = frame; }
  BrowserFrame* frame() const { return frame_; }

  // Returns a pointer to the BrowserView* interface implementation (an
  // instance of this object, typically) for a given native window, or NULL if
  // there is no such association.
  //
  // Don't use this unless you only have a NativeWindow. In nearly all
  // situations plumb through browser and use it.
  static BrowserView* GetBrowserViewForNativeWindow(gfx::NativeWindow window);

  // Returns the BrowserView used for the specified Browser.
  static BrowserView* GetBrowserViewForBrowser(const Browser* browser);

  // Returns a Browser instance of this view.
  Browser* browser() { return browser_.get(); }
  const Browser* browser() const { return browser_.get(); }

  // Initializes (or re-initializes) the status bubble.  We try to only create
  // the bubble once and re-use it for the life of the browser, but certain
  // events (such as changing enabling/disabling Aero on Win) can force a need
  // to change some of the bubble's creation parameters.
  void InitStatusBubble();

  // Initializes the permission bubble view. This class is intended to be
  // created once and then re-used for the life of the browser window. The
  // bubbles it creates will be associated with a single visible tab.
  void InitPermissionBubbleView();

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

  // Bookmark bar may be NULL, for example for pop-ups.
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
  FullscreenExitBubbleViews* fullscreen_exit_bubble() {
    return fullscreen_bubble_.get();
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

  // Restores the focused view. This is also used to set the initial focus
  // when a new browser window is created.
  void RestoreFocus();

  // Called after the widget's fullscreen state is changed without going through
  // FullscreenController. This method does any processing which was skipped.
  // Only exiting fullscreen in this way is currently supported.
  void FullscreenStateChanged();

  // Called from BookmarkBarView/DownloadShelfView during their show/hide
  // animations.
  void ToolbarSizeChanged(bool is_animating);

  // Overridden from BrowserWindow:
  virtual void Show() OVERRIDE;
  virtual void ShowInactive() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual void FlashFrame(bool flash) OVERRIDE;
  virtual bool IsAlwaysOnTop() const OVERRIDE;
  virtual void SetAlwaysOnTop(bool always_on_top) OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;
  virtual BrowserWindowTesting* GetBrowserWindowTesting() OVERRIDE;
  virtual StatusBubble* GetStatusBubble() OVERRIDE;
  virtual void UpdateTitleBar() OVERRIDE;
  virtual void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) OVERRIDE;
  virtual void UpdateDevTools() OVERRIDE;
  virtual void UpdateLoadingAnimations(bool should_animate) OVERRIDE;
  virtual void SetStarredState(bool is_starred) OVERRIDE;
  virtual void SetTranslateIconToggled(bool is_lit) OVERRIDE;
  virtual void OnActiveTabChanged(content::WebContents* old_contents,
                                  content::WebContents* new_contents,
                                  int index,
                                  int reason) OVERRIDE;
  virtual void ZoomChangedForActiveTab(bool can_show_bubble) OVERRIDE;
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual ui::WindowShowState GetRestoredState() const OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual void Maximize() OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual void Restore() OVERRIDE;
  virtual void EnterFullscreen(
      const GURL& url, FullscreenExitBubbleType bubble_type) OVERRIDE;
  virtual void ExitFullscreen() OVERRIDE;
  virtual void UpdateFullscreenExitBubbleContent(
      const GURL& url,
      FullscreenExitBubbleType bubble_type) OVERRIDE;
  virtual bool ShouldHideUIForFullscreen() const OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual bool IsFullscreenBubbleVisible() const OVERRIDE;
#if defined(OS_WIN)
  virtual void SetMetroSnapMode(bool enable) OVERRIDE;
  virtual bool IsInMetroSnapMode() const OVERRIDE;
#endif
  virtual LocationBar* GetLocationBar() const OVERRIDE;
  virtual void SetFocusToLocationBar(bool select_all) OVERRIDE;
  virtual void UpdateReloadStopState(bool is_loading, bool force) OVERRIDE;
  virtual void UpdateToolbar(content::WebContents* contents) OVERRIDE;
  virtual void FocusToolbar() OVERRIDE;
  virtual void FocusAppMenu() OVERRIDE;
  virtual void FocusBookmarksToolbar() OVERRIDE;
  virtual void FocusInfobars() OVERRIDE;
  virtual void RotatePaneFocus(bool forwards) OVERRIDE;
  virtual void DestroyBrowser() OVERRIDE;
  virtual bool IsBookmarkBarVisible() const OVERRIDE;
  virtual bool IsBookmarkBarAnimating() const OVERRIDE;
  virtual bool IsTabStripEditable() const OVERRIDE;
  virtual bool IsToolbarVisible() const OVERRIDE;
  virtual gfx::Rect GetRootWindowResizerRect() const OVERRIDE;
  virtual void ConfirmAddSearchProvider(TemplateURL* template_url,
                                        Profile* profile) OVERRIDE;
  virtual void ShowUpdateChromeDialog() OVERRIDE;
  virtual void ShowBookmarkBubble(const GURL& url,
                                  bool already_bookmarked) OVERRIDE;
  virtual void ShowBookmarkAppBubble(
      const WebApplicationInfo& web_app_info,
      const std::string& extension_id) OVERRIDE;
  virtual void ShowTranslateBubble(
      content::WebContents* contents,
      translate::TranslateStep step,
      translate::TranslateErrors::Type error_type) OVERRIDE;
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  virtual void ShowOneClickSigninBubble(
      OneClickSigninBubbleType type,
      const base::string16& email,
      const base::string16& error_message,
      const StartSyncCallback& start_sync_callback) OVERRIDE;
#endif
  // TODO(beng): Not an override, move somewhere else.
  void SetDownloadShelfVisible(bool visible);
  virtual bool IsDownloadShelfVisible() const OVERRIDE;
  virtual DownloadShelf* GetDownloadShelf() OVERRIDE;
  virtual void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadClosePreventionType dialog_type,
      bool app_modal,
      const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual void UserChangedTheme() OVERRIDE;
  virtual int GetExtraRenderViewHeight() const OVERRIDE;
  virtual void WebContentsFocused(content::WebContents* contents) OVERRIDE;
  virtual void ShowWebsiteSettings(Profile* profile,
                                   content::WebContents* web_contents,
                                   const GURL& url,
                                   const content::SSLStatus& ssl) OVERRIDE;
  virtual void ShowAppMenu() OVERRIDE;
  virtual bool PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual void Cut() OVERRIDE;
  virtual void Copy() OVERRIDE;
  virtual void Paste() OVERRIDE;
  virtual WindowOpenDisposition GetDispositionForPopupBounds(
      const gfx::Rect& bounds) OVERRIDE;
  virtual FindBar* CreateFindBar() OVERRIDE;
  virtual web_modal::WebContentsModalDialogHost*
      GetWebContentsModalDialogHost() OVERRIDE;
  virtual void ShowAvatarBubble(content::WebContents* web_contents,
                                const gfx::Rect& rect) OVERRIDE;
  virtual void ShowAvatarBubbleFromAvatarButton(AvatarBubbleMode mode,
      const signin::ManageAccountsParams& manage_accounts_params) OVERRIDE;
  virtual void ShowPasswordGenerationBubble(
      const gfx::Rect& rect,
      const autofill::PasswordForm& form,
      autofill::PasswordGenerator* password_generator) OVERRIDE;
  virtual void OverscrollUpdate(int delta_y) OVERRIDE;
  virtual int GetRenderViewHeightInsetWithDetachedBookmarkBar() OVERRIDE;
  virtual void ExecuteExtensionCommand(
      const extensions::Extension* extension,
      const extensions::Command& command) OVERRIDE;
  virtual void ShowPageActionPopup(
      const extensions::Extension* extension) OVERRIDE;
  virtual void ShowBrowserActionPopup(
      const extensions::Extension* extension) OVERRIDE;

  // Overridden from BrowserWindowTesting:
  virtual BookmarkBarView* GetBookmarkBarView() const OVERRIDE;
  virtual LocationBarView* GetLocationBarView() const OVERRIDE;
  virtual views::View* GetTabContentsContainerView() const OVERRIDE;
  virtual ToolbarView* GetToolbarView() const OVERRIDE;

  // Overridden from TabStripModelObserver:
  virtual void TabInsertedAt(content::WebContents* contents,
                             int index,
                             bool foreground) OVERRIDE;
  virtual void TabDetachedAt(content::WebContents* contents,
                             int index) OVERRIDE;
  virtual void TabDeactivated(content::WebContents* contents) OVERRIDE;
  virtual void TabStripEmpty() OVERRIDE;
  virtual void WillCloseAllTabs() OVERRIDE;
  virtual void CloseAllTabsCanceled() OVERRIDE;

  // Overridden from ui::AcceleratorProvider:
  virtual bool GetAcceleratorForCommandId(int command_id,
      ui::Accelerator* accelerator) OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual bool CanResize() const OVERRIDE;
  virtual bool CanMaximize() const OVERRIDE;
  virtual bool CanActivate() const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual base::string16 GetAccessibleWindowTitle() const OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual bool ShouldShowWindowTitle() const OVERRIDE;
  virtual gfx::ImageSkia GetWindowAppIcon() OVERRIDE;
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual bool ShouldShowWindowIcon() const OVERRIDE;
  virtual bool ExecuteWindowsCommand(int command_id) OVERRIDE;
  virtual std::string GetWindowName() const OVERRIDE;
  virtual void SaveWindowPlacement(const gfx::Rect& bounds,
                                   ui::WindowShowState show_state) OVERRIDE;
  virtual bool GetSavedWindowPlacement(
      const views::Widget* widget,
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::ClientView* CreateClientView(views::Widget* widget) OVERRIDE;
  virtual void OnWindowBeginUserBoundsChange() OVERRIDE;
  virtual void OnWidgetMove() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual void GetAccessiblePanes(std::vector<View*>* panes) OVERRIDE;

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetActivationChanged(views::Widget* widget,
                                         bool active) OVERRIDE;

  // Overridden from views::ClientView:
  virtual bool CanClose() OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual gfx::Size GetMinimumSize() const OVERRIDE;

  // InfoBarContainer::Delegate overrides
  virtual SkColor GetInfoBarSeparatorColor() const OVERRIDE;
  virtual void InfoBarContainerStateChanged(bool is_animating) OVERRIDE;
  virtual bool DrawInfoBarArrows(int* x) const OVERRIDE;

  // Overridden from views::View:
  virtual const char* GetClassName() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void PaintChildren(gfx::Canvas* canvas,
                             const views::CullSet& cull_set) OVERRIDE;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual void OnNativeThemeChanged(const ui::NativeTheme* theme) OVERRIDE;

  // Overridden from ui::AcceleratorTarget:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

  // OmniboxPopupModelObserver overrides
  virtual void OnOmniboxPopupShownOrHidden() OVERRIDE;

  // Testing interface:
  views::View* GetContentsContainerForTest() { return contents_container_; }
  views::WebView* GetContentsWebViewForTest() { return contents_web_view_; }
  views::WebView* GetDevToolsWebViewForTest() { return devtools_web_view_; }

 private:
  // Do not friend BrowserViewLayout. Use the BrowserViewLayoutDelegate
  // interface to keep these two classes decoupled and testable.
  friend class BrowserViewLayoutDelegateImpl;
  FRIEND_TEST_ALL_PREFIXES(BrowserViewTest, BrowserView);
  FRIEND_TEST_ALL_PREFIXES(BrowserViewsAccessibilityTest,
                           TestAboutChromeViewAccObj);

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
  virtual void OnLoadCompleted() OVERRIDE;

  // Returns the BrowserViewLayout.
  BrowserViewLayout* GetBrowserViewLayout() const;

  // Returns the ContentsLayoutManager.
  ContentsLayoutManager* GetContentsLayoutManager() const;

  // Prepare to show the Bookmark Bar for the specified WebContents.
  // Returns true if the Bookmark Bar can be shown (i.e. it's supported for this
  // Browser type) and there should be a subsequent re-layout to show it.
  // |contents| can be NULL.
  bool MaybeShowBookmarkBar(content::WebContents* contents);

  // Moves the bookmark bar view to the specified parent, which may be NULL,
  // |this|, or |top_container_|. Ensures that |top_container_| stays in front
  // of |bookmark_bar_view_|.
  void SetBookmarkBarParent(views::View* new_parent);

  // Prepare to show an Info Bar for the specified WebContents. Returns
  // true if there is an Info Bar to show and one is supported for this Browser
  // type, and there should be a subsequent re-layout to show it.
  // |contents| can be NULL.
  bool MaybeShowInfoBar(content::WebContents* contents);

  // Updates devtools window for given contents. This method will show docked
  // devtools window for inspected |web_contents| that has docked devtools
  // and hide it for NULL or not inspected |web_contents|. It will also make
  // sure devtools window size and position are restored for given tab.
  // This method will not update actual DevTools WebContents, if not
  // |update_devtools_web_contents|. In this case, manual update is required.
  void UpdateDevToolsForContents(content::WebContents* web_contents,
                                 bool update_devtools_web_contents);

  // Updates various optional child Views, e.g. Bookmarks Bar, Info Bar or the
  // Download Shelf in response to a change notification from the specified
  // |contents|. |contents| can be NULL. In this case, all optional UI will be
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
                         FullscreenExitBubbleType bubble_type);

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

  // Calls |method| which is either WebContents::Cut, ::Copy, or ::Paste,
  // first trying the content WebContents, then the devtools WebContents, and
  // lastly the Views::Textfield if one is focused.
  void DoCutCopyPaste(void (content::WebContents::*method)(),
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

  // The Bookmark Bar View for this window. Lazily created. May be NULL for
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

  // The permission bubble view is the toolkit-specific implementation of the
  // interface used by the manager to display permissions bubbles.
  scoped_ptr<PermissionBubbleViewViews> permission_bubble_view_;

  // A mapping between accelerators and commands.
  std::map<ui::Accelerator, int> accelerator_table_;

  // True if we have already been initialized.
  bool initialized_;

  // True when in ProcessFullscreen(). The flag is used to avoid reentrance and
  // to ignore requests to layout while in ProcessFullscreen() to reduce
  // jankiness.
  bool in_process_fullscreen_;

  scoped_ptr<FullscreenExitBubbleViews> fullscreen_bubble_;

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
  base::RepeatingTimer<BrowserView> loading_animation_timer_;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Used to measure the loading spinner animation rate.
  base::TimeTicks last_animation_time_;

  // If this flag is set then SetFocusToLocationBar() will set focus to the
  // location bar even if the browser window is not active.
  bool force_location_bar_focus_;

  scoped_ptr<ImmersiveModeController> immersive_mode_controller_;

  scoped_ptr<ScrollEndEffectController> scroll_end_effect_controller_;

  scoped_ptr<WebContentsCloseHandler> web_contents_close_handler_;

  mutable base::WeakPtrFactory<BrowserView> activate_modal_dialog_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_H_
