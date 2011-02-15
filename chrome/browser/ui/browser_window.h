// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_H_
#pragma once

#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/common/content_settings_types.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class BrowserWindowTesting;
class DownloadShelf;
class FindBar;
class GURL;
class HtmlDialogUIDelegate;
class LocationBar;
class Profile;
class StatusBubble;
class TabContents;
class TabContentsWrapper;
class TemplateURL;
class TemplateURLModel;
#if !defined(OS_MACOSX)
class ToolbarView;
#endif
struct NativeWebKeyboardEvent;

namespace gfx {
class Rect;
}

namespace views {
class Window;
}

class Extension;

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow interface
//  An interface implemented by the "view" of the Browser window.
//
// NOTE: All getters may return NULL.
class BrowserWindow {
 public:
  // Show the window, or activates it if it's already visible.
  virtual void Show() = 0;

  // Sets the window's size and position to the specified values.
  virtual void SetBounds(const gfx::Rect& bounds) = 0;

  // Closes the frame as soon as possible.  If the frame is not in a drag
  // session, it will close immediately; otherwise, it will move offscreen (so
  // events are still fired) until the drag ends, then close. This assumes
  // that the Browser is not immediately destroyed, but will be eventually
  // destroyed by other means (eg, the tab strip going to zero elements).
  // Bad things happen if the Browser dtor is called directly as a result of
  // invoking this method.
  virtual void Close() = 0;

  // Activates (brings to front) the window. Restores the window from minimized
  // state if necessary.
  virtual void Activate() = 0;

  // Deactivates the window, making the next window in the Z order the active
  // window.
  virtual void Deactivate() = 0;

  // Returns true if the window is currently the active/focused window.
  virtual bool IsActive() const = 0;

  // Flashes the taskbar item associated with this frame.
  virtual void FlashFrame() = 0;

  // Return a platform dependent identifier for this frame. On Windows, this
  // returns an HWND.
  virtual gfx::NativeWindow GetNativeHandle() = 0;

  // Returns a pointer to the testing interface to the Browser window, or NULL
  // if there is none.
  virtual BrowserWindowTesting* GetBrowserWindowTesting() = 0;

  // Return the status bubble associated with the frame
  virtual StatusBubble* GetStatusBubble() = 0;

  // Inform the receiving frame that an animation has progressed in the
  // selected tab.
  // TODO(beng): Remove. Infobars/Boomarks bars should talk directly to
  //             BrowserView.
  virtual void SelectedTabToolbarSizeChanged(bool is_animating) = 0;

  // Inform the frame that the selected tab favicon or title has changed. Some
  // frames may need to refresh their title bar.
  virtual void UpdateTitleBar() = 0;

  // Invoked when the visibility of the bookmark bar.
  // NOTE: this is NOT sent when the user toggles the visibility of this,
  // but rather when the user transitions from a page that forces
  // it to be visibile to one that doesn't have it visible (or
  // vice-versa).
  // TODO(sky): see about routing visibility pref changing through here too.
  virtual void ShelfVisibilityChanged() = 0;

  // Inform the frame that the dev tools window for the selected tab has
  // changed.
  virtual void UpdateDevTools() = 0;

  // Update any loading animations running in the window. |should_animate| is
  // true if there are tabs loading and the animations should continue, false
  // if there are no active loads and the animations should end.
  virtual void UpdateLoadingAnimations(bool should_animate) = 0;

  // Sets the starred state for the current tab.
  virtual void SetStarredState(bool is_starred) = 0;

  // Returns the nonmaximized bounds of the frame (even if the frame is
  // currently maximized or minimized) in terms of the screen coordinates.
  virtual gfx::Rect GetRestoredBounds() const = 0;

  // Retrieves the window's current bounds, including its frame.
  // This will only differ from GetRestoredBounds() for maximized
  // and minimized windows.
  virtual gfx::Rect GetBounds() const = 0;

  // TODO(beng): REMOVE?
  // Returns true if the frame is maximized (aka zoomed).
  virtual bool IsMaximized() const = 0;

  // Accessors for fullscreen mode state.
  virtual void SetFullscreen(bool fullscreen) = 0;
  virtual bool IsFullscreen() const = 0;

  // Returns true if the fullscreen bubble is visible.
  virtual bool IsFullscreenBubbleVisible() const = 0;

  // Returns the location bar.
  virtual LocationBar* GetLocationBar() const = 0;

  // Tries to focus the location bar.  Clears the window focus (to avoid
  // inconsistent state) if this fails.
  virtual void SetFocusToLocationBar(bool select_all) = 0;

  // Informs the view whether or not a load is in progress for the current tab.
  // The view can use this notification to update the reload/stop button.
  virtual void UpdateReloadStopState(bool is_loading, bool force) = 0;

  // Updates the toolbar with the state for the specified |contents|.
  virtual void UpdateToolbar(TabContentsWrapper* contents,
                             bool should_restore_state) = 0;

  // Focuses the toolbar (for accessibility).
  virtual void FocusToolbar() = 0;

  // Focuses the app menu like it was a menu bar.
  //
  // Not used on the Mac, which has a "normal" menu bar.
  virtual void FocusAppMenu() = 0;

  // Focuses the bookmarks toolbar (for accessibility).
  virtual void FocusBookmarksToolbar() = 0;

  // Focuses the Chrome OS status view (for accessibility).
  virtual void FocusChromeOSStatus() = 0;

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

  // Tells the frame not to render as inactive until the next activation change.
  // This is required on Windows when dropdown selects are shown to prevent the
  // select from deactivating the browser frame. A stub implementation is
  // provided here since the functionality is Windows-specific.
  virtual void DisableInactiveFrame() {}

  // Shows a confirmation dialog box for setting the default search engine
  // described by |template_url|. Takes ownership of |template_url|.
  virtual void ConfirmSetDefaultSearchProvider(
      TabContentsWrapper* tab_contents,
      TemplateURL* template_url,
      TemplateURLModel* template_url_model) {
    // TODO(levin): Implement this for non-Windows platforms and make it pure.
    // http://crbug.com/38475
  }

  // Shows a confirmation dialog box for adding a search engine described by
  // |template_url|. Takes ownership of |template_url|.
  virtual void ConfirmAddSearchProvider(const TemplateURL* template_url,
                                        Profile* profile) = 0;

  // Shows or hides the bookmark bar depending on its current visibility.
  virtual void ToggleBookmarkBar() = 0;

  // Shows the About Chrome dialog box.
  virtual views::Window* ShowAboutChromeDialog() = 0;

  // Shows the Update Recommended dialog box.
  virtual void ShowUpdateChromeDialog() = 0;

  // Shows the Task manager.
  virtual void ShowTaskManager() = 0;

  // Shows task information related to background pages.
  virtual void ShowBackgroundPages() = 0;

  // Shows the Bookmark bubble. |url| is the URL being bookmarked,
  // |already_bookmarked| is true if the url is already bookmarked.
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked) = 0;

  // Whether or not the shelf view is visible.
  virtual bool IsDownloadShelfVisible() const = 0;

  // Returns the DownloadShelf.
  virtual DownloadShelf* GetDownloadShelf() = 0;

  // Shows the Clear Browsing Data dialog box.
  virtual void ShowClearBrowsingDataDialog() = 0;

  // Shows the Import Bookmarks & Settings dialog box.
  virtual void ShowImportDialog() = 0;

  // Shows the Search Engines dialog box.
  virtual void ShowSearchEnginesDialog() = 0;

  // Shows the Password Manager dialog box.
  virtual void ShowPasswordManager() = 0;

  // Shows the repost form confirmation dialog box.
  virtual void ShowRepostFormWarningDialog(TabContents* tab_contents) = 0;

  // Shows the Content Settings dialog box.
  virtual void ShowContentSettingsWindow(ContentSettingsType content_type,
                                         Profile* profile) = 0;

  // Shows the collected cookies dialog box.
  virtual void ShowCollectedCookiesDialog(TabContents* tab_contents) = 0;

  // Shows a dialog to the user that something is wrong with the profile.
  // |message_id| is the ID for a string in the string table which will be
  // displayed in the dialog.
  virtual void ShowProfileErrorDialog(int message_id) = 0;

  // Show the bubble that indicates to the user that a theme is being installed.
  virtual void ShowThemeInstallBubble() = 0;

  // Shows the confirmation dialog box warning that the browser is closing with
  // in-progress downloads.
  // This method should call Browser::InProgressDownloadResponse once the user
  // has confirmed.
  virtual void ConfirmBrowserCloseWithPendingDownloads() = 0;

  // Shows a dialog box with HTML content, e.g. for Gears. |parent_window| is
  // the window the dialog should be opened modal to and is a native window
  // handle.
  virtual void ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
                              gfx::NativeWindow parent_window) = 0;

  // BrowserThemeProvider calls this when a user has changed his or her theme,
  // indicating that it's time to redraw everything.
  virtual void UserChangedTheme() = 0;

  // Get extra vertical height that the render view should add to its requests
  // to webkit. This can help prevent sending extraneous layout/repaint requests
  // when the delegate is in the process of resizing the tab contents view (e.g.
  // during infobar animations).
  virtual int GetExtraRenderViewHeight() const = 0;

  // Notification that |tab_contents| got the focus through user action (click
  // on the page).
  virtual void TabContentsFocused(TabContents* tab_contents) = 0;

  // Shows the page info using the specified information.
  // |url| is the url of the page/frame the info applies to, |ssl| is the SSL
  // information for that page/frame.  If |show_history| is true, a section
  // showing how many times that URL has been visited is added to the page info.
  virtual void ShowPageInfo(Profile* profile,
                            const GURL& url,
                            const NavigationEntry::SSLStatus& ssl,
                            bool show_history) = 0;

  // Shows the app menu (for accessibility).
  virtual void ShowAppMenu() = 0;

  // Allows the BrowserWindow object to handle the specified keyboard event
  // before sending it to the renderer.
  // Returns true if the |event| was handled. Otherwise, if the |event| would
  // be handled in HandleKeyboardEvent() method as a normal keyboard shortcut,
  // |*is_keyboard_shortcut| should be set to true.
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut) = 0;

  // Allows the BrowserWindow object to handle the specified keyboard event,
  // if the renderer did not process it.
  virtual void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) = 0;

  // Shows the create web app shortcut dialog box.
  virtual void ShowCreateWebAppShortcutsDialog(TabContents* tab_contents) = 0;

  // Shows the create chrome app shortcut dialog box.
  virtual void ShowCreateChromeAppShortcutsDialog(Profile* profile,
                                                  const Extension* app) = 0;

  // Clipboard commands applied to the whole browser window.
  virtual void Cut() = 0;
  virtual void Copy() = 0;
  virtual void Paste() = 0;

  // Switches between available tabstrip display modes.
  virtual void ToggleTabStripMode() = 0;

#if defined(OS_MACOSX)
  // Opens the tabpose view.
  virtual void OpenTabpose() = 0;
#endif

  // See InstantDelegate for details.
  virtual void PrepareForInstant() = 0;

  // Invoked when instant's tab contents should be shown.
  virtual void ShowInstant(TabContents* preview_contents) = 0;

  // Invoked when the instant's tab contents should be hidden.
  // |instant_is_active| indicates if instant is still active.
  virtual void HideInstant(bool instant_is_active) = 0;

  // Returns the desired bounds for instant in screen coordinates. Note that if
  // instant isn't currently visible this returns the bounds instant would be
  // placed at.
  virtual gfx::Rect GetInstantBounds() = 0;

#if defined(OS_CHROMEOS)
  // Shows the keyboard overlay dialog box.
  virtual void ShowKeyboardOverlay(gfx::NativeWindow owning_window) = 0;
#endif

  // Construct a BrowserWindow implementation for the specified |browser|.
  static BrowserWindow* CreateBrowserWindow(Browser* browser);

  // Construct a FindBar implementation for the specified |browser|.
  static FindBar* CreateFindBar(Browser* browser_window);

 protected:
  friend class BrowserList;
  friend class BrowserView;
  virtual void DestroyBrowser() = 0;

  virtual ~BrowserWindow() {}
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

  // Returns the TabContentsContainer.
  virtual views::View* GetSidebarContainerView() const = 0;

  // Returns the ToolbarView.
  virtual ToolbarView* GetToolbarView() const = 0;
#endif

 protected:
  virtual ~BrowserWindowTesting() {}
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_H_
