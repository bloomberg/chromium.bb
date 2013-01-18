// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_H_

#include "base/callback_forward.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/base_window.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/fullscreen/fullscreen_exit_bubble_type.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "chrome/common/content_settings_types.h"
#include "ui/gfx/native_widget_types.h"
#include "webkit/glue/window_open_disposition.h"

class Browser;
class BrowserWindowTesting;
class DownloadShelf;
class FindBar;
class GURL;
class LocationBar;
class Profile;
class StatusBubble;
class TemplateURL;
#if !defined(OS_MACOSX)
class ToolbarView;
#endif

namespace autofill {
class PasswordGenerator;
}
namespace content {
class WebContents;
struct NativeWebKeyboardEvent;
struct PasswordForm;
struct SSLStatus;
}

namespace extensions {
class Extension;
}

namespace gfx {
class Rect;
class Size;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow interface
//  An interface implemented by the "view" of the Browser window.
//  This interface includes BaseWindow methods as well as Browser window
//  specific methods.
//
// NOTE: All getters may return NULL.
//
class BrowserWindow : public BaseWindow {
 public:
  virtual ~BrowserWindow() {}

  //////////////////////////////////////////////////////////////////////////////
  // BaseWindow interface notes:

  // Closes the window as soon as possible. If the window is not in a drag
  // session, it will close immediately; otherwise, it will move offscreen (so
  // events are still fired) until the drag ends, then close. This assumes
  // that the Browser is not immediately destroyed, but will be eventually
  // destroyed by other means (eg, the tab strip going to zero elements).
  // Bad things happen if the Browser dtor is called directly as a result of
  // invoking this method.
  // virtual void Close() = 0;

  // Browser::OnWindowDidShow should be called after showing the window.
  // virtual void Show() = 0;

  //////////////////////////////////////////////////////////////////////////////
  // Browser specific methods:

  // Returns a pointer to the testing interface to the Browser window, or NULL
  // if there is none.
  virtual BrowserWindowTesting* GetBrowserWindowTesting() = 0;

  // Return the status bubble associated with the frame
  virtual StatusBubble* GetStatusBubble() = 0;

  // Inform the frame that the selected tab favicon or title has changed. Some
  // frames may need to refresh their title bar.
  virtual void UpdateTitleBar() = 0;

  // Invoked when the state of the bookmark bar changes. This is only invoked if
  // the state changes for the current tab, it is not sent when switching tabs.
  virtual void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) = 0;

  // Inform the frame that the dev tools window for the selected tab has
  // changed.
  virtual void UpdateDevTools() = 0;

  // Update any loading animations running in the window. |should_animate| is
  // true if there are tabs loading and the animations should continue, false
  // if there are no active loads and the animations should end.
  virtual void UpdateLoadingAnimations(bool should_animate) = 0;

  // Sets the starred state for the current tab.
  virtual void SetStarredState(bool is_starred) = 0;

  // Called to force the zoom state to for the active tab to be recalculated.
  // |can_show_bubble| is true when a user presses the zoom up or down keyboard
  // shortcuts and will be false in other cases (e.g. switching tabs, "clicking"
  // + or - in the wrench menu to change zoom).
  virtual void ZoomChangedForActiveTab(bool can_show_bubble) = 0;

  // Accessors for fullscreen mode state.
  virtual void EnterFullscreen(const GURL& url,
                               FullscreenExitBubbleType bubble_type) = 0;
  virtual void ExitFullscreen() = 0;
  virtual void UpdateFullscreenExitBubbleContent(
      const GURL& url,
      FullscreenExitBubbleType bubble_type) = 0;

  // Returns true if the fullscreen bubble is visible.
  virtual bool IsFullscreenBubbleVisible() const = 0;

#if defined(OS_WIN)
  // Sets state for entering or exiting Win8 Metro snap mode.
  virtual void SetMetroSnapMode(bool enable) = 0;

  // Returns whether the window is currently in Win8 Metro snap mode.
  virtual bool IsInMetroSnapMode() const = 0;
#endif

  // Returns the location bar.
  virtual LocationBar* GetLocationBar() const = 0;

  // Tries to focus the location bar.  Clears the window focus (to avoid
  // inconsistent state) if this fails.
  virtual void SetFocusToLocationBar(bool select_all) = 0;

  // Informs the view whether or not a load is in progress for the current tab.
  // The view can use this notification to update the reload/stop button.
  virtual void UpdateReloadStopState(bool is_loading, bool force) = 0;

  // Updates the toolbar with the state for the specified |contents|.
  virtual void UpdateToolbar(content::WebContents* contents,
                             bool should_restore_state) = 0;

  // Focuses the toolbar (for accessibility).
  virtual void FocusToolbar() = 0;

  // Focuses the app menu like it was a menu bar.
  //
  // Not used on the Mac, which has a "normal" menu bar.
  virtual void FocusAppMenu() = 0;

  // Focuses the bookmarks toolbar (for accessibility).
  virtual void FocusBookmarksToolbar() = 0;

  // Moves keyboard focus to the next pane.
  virtual void RotatePaneFocus(bool forwards) = 0;

  // Returns whether the bookmark bar is visible or not.
  virtual bool IsBookmarkBarVisible() const = 0;

  // Returns whether the bookmark bar is animating or not.
  virtual bool IsBookmarkBarAnimating() const = 0;

  // Returns whether the tab strip is editable (for extensions).
  virtual bool IsTabStripEditable() const = 0;

  // Returns whether the tool bar is visible or not.
  virtual bool IsToolbarVisible() const = 0;

  // Returns the rect where the resize corner should be drawn by the render
  // widget host view (on top of what the renderer returns). We return an empty
  // rect to identify that there shouldn't be a resize corner (in the cases
  // where we take care of it ourselves at the browser level).
  virtual gfx::Rect GetRootWindowResizerRect() const = 0;

  // Returns whether the window is a panel. This is not always synonomous
  // with the associated browser having type panel since some environments
  // may draw popups in panel windows.
  virtual bool IsPanel() const = 0;

  // Tells the frame not to render as inactive until the next activation change.
  // This is required on Windows when dropdown selects are shown to prevent the
  // select from deactivating the browser frame. A stub implementation is
  // provided here since the functionality is Windows-specific.
  virtual void DisableInactiveFrame() {}

  // Shows a confirmation dialog box for adding a search engine described by
  // |template_url|. Takes ownership of |template_url|.
  virtual void ConfirmAddSearchProvider(TemplateURL* template_url,
                                        Profile* profile) = 0;

  // Shows or hides the bookmark bar depending on its current visibility.
  virtual void ToggleBookmarkBar() = 0;

  // Shows the Update Recommended dialog box.
  virtual void ShowUpdateChromeDialog() = 0;

  // Shows the Task manager.
  virtual void ShowTaskManager() = 0;

  // Shows task information related to background pages.
  virtual void ShowBackgroundPages() = 0;

  // Shows the Bookmark bubble. |url| is the URL being bookmarked,
  // |already_bookmarked| is true if the url is already bookmarked.
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked) = 0;

  // Shows the bookmark prompt.
  // TODO(yosin): Make ShowBookmarkPrompt pure virtual.
  virtual void ShowBookmarkPrompt() {}

  // Shows the Chrome To Mobile bubble.
  virtual void ShowChromeToMobileBubble() = 0;

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  // Callback type used with the ShowOneClickSigninBubble() method.  If the
  // user chooses to accept the sign in, the callback is called to start the
  // sync process.
  typedef base::Callback<void(OneClickSigninSyncStarter::StartSyncMode)>
      StartSyncCallback;

  // Shows the one-click sign in bubble.
  virtual void ShowOneClickSigninBubble(
      const StartSyncCallback& start_sync_callback) = 0;
#endif

  // Whether or not the shelf view is visible.
  virtual bool IsDownloadShelfVisible() const = 0;

  // Returns the DownloadShelf.
  virtual DownloadShelf* GetDownloadShelf() = 0;

  // Shows the confirmation dialog box warning that the browser is closing with
  // in-progress downloads.
  // This method should call Browser::InProgressDownloadResponse once the user
  // has confirmed.
  virtual void ConfirmBrowserCloseWithPendingDownloads() = 0;

  // ThemeService calls this when a user has changed his or her theme,
  // indicating that it's time to redraw everything.
  virtual void UserChangedTheme() = 0;

  // Get extra vertical height that the render view should add to its requests
  // to webkit. This can help prevent sending extraneous layout/repaint requests
  // when the delegate is in the process of resizing the tab contents view (e.g.
  // during infobar animations).
  virtual int GetExtraRenderViewHeight() const = 0;

  // Notification that |contents| got the focus through user action (click
  // on the page).
  virtual void WebContentsFocused(content::WebContents* contents) = 0;

  // Shows the page info using the specified information.
  // |url| is the url of the page/frame the info applies to, |ssl| is the SSL
  // information for that page/frame.  If |show_history| is true, a section
  // showing how many times that URL has been visited is added to the page info.
  virtual void ShowPageInfo(content::WebContents* web_contents,
                            const GURL& url,
                            const content::SSLStatus& ssl,
                            bool show_history) = 0;

  // Shows the website settings using the specified information. |url| is the
  // url of the page/frame the info applies to, |ssl| is the SSL information for
  // that page/frame.  If |show_history| is true, a section showing how many
  // times that URL has been visited is added to the page info.
  virtual void ShowWebsiteSettings(Profile* profile,
                                   content::WebContents* web_contents,
                                   const GURL& url,
                                   const content::SSLStatus& ssl,
                                   bool show_history) = 0;

  // Shows the app menu (for accessibility).
  virtual void ShowAppMenu() = 0;

  // Allows the BrowserWindow object to handle the specified keyboard event
  // before sending it to the renderer.
  // Returns true if the |event| was handled. Otherwise, if the |event| would
  // be handled in HandleKeyboardEvent() method as a normal keyboard shortcut,
  // |*is_keyboard_shortcut| should be set to true.
  virtual bool PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) = 0;

  // Allows the BrowserWindow object to handle the specified keyboard event,
  // if the renderer did not process it.
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) = 0;

  // Shows the create chrome app shortcut dialog box.
  virtual void ShowCreateChromeAppShortcutsDialog(Profile* profile,
      const extensions::Extension* app) = 0;

  // Clipboard commands applied to the whole browser window.
  virtual void Cut() = 0;
  virtual void Copy() = 0;
  virtual void Paste() = 0;

#if defined(OS_MACOSX)
  // Opens the tabpose view.
  virtual void OpenTabpose() = 0;

  // Sets the presentation mode for the window.  If the window is not already in
  // fullscreen, also enters fullscreen mode.
  virtual void EnterPresentationMode(
      const GURL& url,
      FullscreenExitBubbleType bubble_type) = 0;
  virtual void ExitPresentationMode() = 0;
  virtual bool InPresentationMode() = 0;
#endif

  // Returns the desired bounds for Instant in screen coordinates. Note that if
  // Instant isn't currently visible this returns the bounds Instant would be
  // placed at.
  virtual gfx::Rect GetInstantBounds() = 0;

  // Return the correct disposition for a popup window based on |bounds|.
  virtual WindowOpenDisposition GetDispositionForPopupBounds(
      const gfx::Rect& bounds) = 0;

  // Construct a FindBar implementation for the |browser|.
  virtual FindBar* CreateFindBar() = 0;

  // Updates the |top_y| where the top of the constrained window should be
  // positioned. When implemented, the method returns true and the value of
  // |top_y| is non-negative. When not implemented, the method returns false and
  // the value of |top_y| is not defined.
  virtual bool GetConstrainedWindowTopY(int* top_y) = 0;

  // Invoked when the preferred size of the contents in current tab has been
  // changed. We might choose to update the window size to accomodate this
  // change.
  // Note that this won't be fired if we change tabs.
  virtual void UpdatePreferredSize(content::WebContents* web_contents,
                                   const gfx::Size& pref_size) {}

  // Invoked when the contents auto-resized and the container should match it.
  virtual void ResizeDueToAutoResize(content::WebContents* web_contents,
                                     const gfx::Size& new_size) {}

  // Construct a BrowserWindow implementation for the specified |browser|.
  static BrowserWindow* CreateBrowserWindow(Browser* browser);

  // Shows the avatar bubble inside |web_contents|. The bubble is positioned
  // relative to |rect|. |rect| should be in the |web_contents| coordinate
  // system.
  virtual void ShowAvatarBubble(content::WebContents* web_contents,
                                const gfx::Rect& rect) = 0;

  // Shows the avatar bubble on the window frame off of the avatar button.
  virtual void ShowAvatarBubbleFromAvatarButton() = 0;

  // Show bubble for password generation positioned relative to |rect|. The
  // subclasses implementing this interface do not own the |password_generator|
  // object which is passed to generate the password. |form| is the form that
  // contains the password field that the bubble will be associated with.
  virtual void ShowPasswordGenerationBubble(
      const gfx::Rect& rect,
      const content::PasswordForm& form,
      autofill::PasswordGenerator* password_generator) = 0;

 protected:
  friend void browser::CloseAllBrowsers();
  friend class BrowserView;
  virtual void DestroyBrowser() = 0;
};

#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
class BookmarkBarView;
class LocationBarView;

namespace views {
class View;
}
#endif  // defined(OS_WIN)

// A BrowserWindow utility interface used for accessing elements of the browser
// UI used only by UI test automation.
class BrowserWindowTesting {
 public:
#if defined(OS_WIN) || defined(TOOLKIT_VIEWS)
  // Returns the BookmarkBarView.
  virtual BookmarkBarView* GetBookmarkBarView() const = 0;

  // Returns the LocationBarView.
  virtual LocationBarView* GetLocationBarView() const = 0;

  // Returns the TabContentsContainer.
  virtual views::View* GetTabContentsContainerView() const = 0;

  // Returns the ToolbarView.
  virtual ToolbarView* GetToolbarView() const = 0;
#endif

 protected:
  virtual ~BrowserWindowTesting() {}
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_H_
