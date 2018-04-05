// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/browser_close_manager.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"
#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"
#include "chrome/common/buildflags.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/translate/core/common/translate_errors.h"
#include "ui/base/base_window.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/apps/intent_helper/apps_navigation_types.h"
#endif  // defined(OS_CHROMEOS)

class Browser;
class DownloadShelf;
class ExclusiveAccessContext;
class FindBar;
class GURL;
class LocationBar;
class StatusBubble;
class ToolbarActionsBar;

namespace autofill {
class SaveCardBubbleController;
class SaveCardBubbleView;
}

namespace content {
class WebContents;
struct NativeWebKeyboardEvent;
enum class KeyboardEventProcessingResult;
}

namespace extensions {
class Command;
class Extension;
}

namespace gfx {
class Rect;
class Size;
}

namespace signin_metrics {
enum class AccessPoint;
}

namespace web_modal {
class WebContentsModalDialogHost;
}

enum class ImeWarningBubblePermissionStatus;

enum class ShowTranslateBubbleResult {
  // The translate bubble was successfully shown.
  SUCCESS,

  // The various reasons for which the translate bubble could fail to be shown.
  BROWSER_WINDOW_NOT_VALID,
  BROWSER_WINDOW_MINIMIZED,
  BROWSER_WINDOW_NOT_ACTIVE,
  WEB_CONTENTS_NOT_ACTIVE,
  EDITABLE_FIELD_IS_ACTIVE,
};

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow interface
//  An interface implemented by the "view" of the Browser window.
//  This interface includes ui::BaseWindow methods as well as Browser window
//  specific methods.
//
// NOTE: All getters may return NULL.
//
class BrowserWindow : public ui::BaseWindow {
 public:
  virtual ~BrowserWindow() {}

  //////////////////////////////////////////////////////////////////////////////
  // ui::BaseWindow interface notes:

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

  // Sets whether the translate icon is lit for the current tab.
  virtual void SetTranslateIconToggled(bool is_lit) = 0;

  // Called when the active tab changes.  Subclasses which implement
  // TabStripModelObserver should implement this instead of ActiveTabChanged();
  // the Browser will call this method while processing that one.
  virtual void OnActiveTabChanged(content::WebContents* old_contents,
                                  content::WebContents* new_contents,
                                  int index,
                                  int reason) = 0;

  // Called to force the zoom state to for the active tab to be recalculated.
  // |can_show_bubble| is true when a user presses the zoom up or down keyboard
  // shortcuts and will be false in other cases (e.g. switching tabs, "clicking"
  // + or - in the app menu to change zoom).
  virtual void ZoomChangedForActiveTab(bool can_show_bubble) = 0;

  // Windows and GTK remove the browser controls in fullscreen, but Mac and Ash
  // keep the controls in a slide-down panel.
  virtual bool ShouldHideUIForFullscreen() const = 0;

  // Returns true if the fullscreen bubble is visible.
  virtual bool IsFullscreenBubbleVisible() const = 0;

  // Returns the size of WebContents in the browser. This may be called before
  // the TabStripModel has an active tab.
  virtual gfx::Size GetContentsSize() const = 0;

  // Returns the location bar.
  virtual LocationBar* GetLocationBar() const = 0;

  // Tries to focus the location bar.  Clears the window focus (to avoid
  // inconsistent state) if this fails.
  virtual void SetFocusToLocationBar(bool select_all) = 0;

  // Informs the view whether or not a load is in progress for the current tab.
  // The view can use this notification to update the reload/stop button.
  virtual void UpdateReloadStopState(bool is_loading, bool force) = 0;

  // Updates the toolbar with the state for the specified |contents|.
  virtual void UpdateToolbar(content::WebContents* contents) = 0;

  // Resets the toolbar's tab state for |contents|.
  virtual void ResetToolbarTabState(content::WebContents* contents) = 0;

  // Focuses the toolbar (for accessibility).
  virtual void FocusToolbar() = 0;

  // Returns the ToolbarActionsBar associated with the window, if any.
  virtual ToolbarActionsBar* GetToolbarActionsBar() = 0;

  // Called from toolbar subviews during their show/hide animations.
  virtual void ToolbarSizeChanged(bool is_animating) = 0;

  // Focuses the app menu like it was a menu bar.
  //
  // Not used on the Mac, which has a "normal" menu bar.
  virtual void FocusAppMenu() = 0;

  // Focuses the bookmarks toolbar (for accessibility).
  virtual void FocusBookmarksToolbar() = 0;

  // Focuses a visible but inactive popup for accessibility.
  virtual void FocusInactivePopupForAccessibility() = 0;

  // Moves keyboard focus to the next pane.
  virtual void RotatePaneFocus(bool forwards) = 0;

  // Returns whether the bookmark bar is visible or not.
  virtual bool IsBookmarkBarVisible() const = 0;

  // Returns whether the bookmark bar is animating or not.
  virtual bool IsBookmarkBarAnimating() const = 0;

  // Returns whether the tab strip is editable (for extensions).
  virtual bool IsTabStripEditable() const = 0;

  // Returns whether the toolbar is available or not. It's called "Visible()"
  // to follow the name convention. But it does not indicate the visibility of
  // the toolbar, i.e. toolbar may be hidden, and only visible when the mouse
  // cursor is at a certain place.
  // TODO(zijiehe): Rename Visible() functions into Available() to match their
  // original meaning.
  virtual bool IsToolbarVisible() const = 0;

  // Returns whether the toolbar is showing up on the screen.
  // TODO(zijiehe): Rename this function into IsToolbarVisible() once other
  // Visible() functions are renamed to Available().
  virtual bool IsToolbarShowing() const = 0;

  // Shows the Update Recommended dialog box.
  virtual void ShowUpdateChromeDialog() = 0;

#if defined(OS_CHROMEOS)
  // Shows the intent picker bubble. |app_info| contains the app candidates to
  // display and |callback| gives access so we can redirect the user (if needed)
  // and store UMA metrics.
  virtual void ShowIntentPickerBubble(
      std::vector<chromeos::IntentPickerAppInfo> app_info,
      IntentPickerResponse callback) = 0;
  virtual void SetIntentPickerViewVisibility(bool visible) = 0;
#endif  // defined(OS_CHROMEOS)

  // Shows the Bookmark bubble. |url| is the URL being bookmarked,
  // |already_bookmarked| is true if the url is already bookmarked.
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked) = 0;

  // Shows the "Save credit card" bubble.
  virtual autofill::SaveCardBubbleView* ShowSaveCreditCardBubble(
      content::WebContents* contents,
      autofill::SaveCardBubbleController* controller,
      bool is_user_gesture) = 0;

  // Shows the translate bubble.
  //
  // |is_user_gesture| is true when the bubble is shown on the user's deliberate
  // action.
  virtual ShowTranslateBubbleResult ShowTranslateBubble(
      content::WebContents* contents,
      translate::TranslateStep step,
      translate::TranslateErrors::Type error_type,
      bool is_user_gesture) = 0;

#if BUILDFLAG(ENABLE_ONE_CLICK_SIGNIN)
  // Callback type used with the ShowOneClickSigninConfirmation() method. If the
  // user chooses to accept the sign in, the callback is called to start the
  // sync process.
  typedef base::Callback<void(OneClickSigninSyncStarter::StartSyncMode)>
      StartSyncCallback;

  // Shows the one-click sign in confirmation UI. |email| holds the full email
  // address of the account that has signed in.
  virtual void ShowOneClickSigninConfirmation(
      const base::string16& email,
      const StartSyncCallback& start_sync_callback) = 0;
#endif

  // Whether or not the shelf view is visible.
  virtual bool IsDownloadShelfVisible() const = 0;

  // Returns the DownloadShelf.
  virtual DownloadShelf* GetDownloadShelf() = 0;

  // Shows the confirmation dialog box warning that the browser is closing with
  // in-progress downloads.
  // This method should call |callback| with the user's response.
  virtual void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadClosePreventionType dialog_type,
      bool app_modal,
      const base::Callback<void(bool)>& callback) = 0;

  // ThemeService calls this when a user has changed their theme, indicating
  // that it's time to redraw everything.
  virtual void UserChangedTheme() = 0;

  // Shows the app menu (for accessibility).
  virtual void ShowAppMenu() = 0;

  // Allows the BrowserWindow object to handle the specified keyboard event
  // before sending it to the renderer.
  virtual content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) = 0;

  // Allows the BrowserWindow object to handle the specified keyboard event,
  // if the renderer did not process it.
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) = 0;

  // Clipboard commands applied to the whole browser window.
  virtual void CutCopyPaste(int command_id) = 0;

  // Return the correct disposition for a popup window based on |bounds|.
  virtual WindowOpenDisposition GetDispositionForPopupBounds(
      const gfx::Rect& bounds) = 0;

  // Construct a FindBar implementation for the |browser|.
  virtual FindBar* CreateFindBar() = 0;

  // Return the WebContentsModalDialogHost for use in positioning web contents
  // modal dialogs within the browser window. This can sometimes be NULL (for
  // instance during tab drag on Views/Win32).
  virtual web_modal::WebContentsModalDialogHost*
      GetWebContentsModalDialogHost() = 0;

  // Construct a BrowserWindow implementation for the specified |browser|.
  static BrowserWindow* CreateBrowserWindow(Browser* browser,
                                            bool user_gesture);

  // Shows the avatar bubble on the window frame off of the avatar button with
  // the given mode. The Service Type specified by GAIA is provided as well.
  // |access_point| indicates the access point used to open the Gaia sign in
  // page.
  enum AvatarBubbleMode {
    AVATAR_BUBBLE_MODE_DEFAULT,
    AVATAR_BUBBLE_MODE_ACCOUNT_MANAGEMENT,
    AVATAR_BUBBLE_MODE_SIGNIN,
    AVATAR_BUBBLE_MODE_ADD_ACCOUNT,
    AVATAR_BUBBLE_MODE_REAUTH,
    AVATAR_BUBBLE_MODE_CONFIRM_SIGNIN,
    AVATAR_BUBBLE_MODE_SHOW_ERROR,
  };
  virtual void ShowAvatarBubbleFromAvatarButton(
      AvatarBubbleMode mode,
      const signin::ManageAccountsParams& manage_accounts_params,
      signin_metrics::AccessPoint access_point,
      bool is_source_keyboard) = 0;

  // Returns the height inset for RenderView when detached bookmark bar is
  // shown.  Invoked when a new RenderHostView is created for a non-NTP
  // navigation entry and the bookmark bar is detached.
  virtual int GetRenderViewHeightInsetWithDetachedBookmarkBar() = 0;

  // Executes |command| registered by |extension|.
  virtual void ExecuteExtensionCommand(const extensions::Extension* extension,
                                       const extensions::Command& command) = 0;

  // Returns object implementing ExclusiveAccessContext interface.
  virtual ExclusiveAccessContext* GetExclusiveAccessContext() = 0;

  // Shows the IME warning bubble.
  virtual void ShowImeWarningBubble(
      const extensions::Extension* extension,
      const base::Callback<void(ImeWarningBubblePermissionStatus status)>&
          callback) = 0;

  // Returns the platform-specific ID of the workspace the browser window
  // currently resides in.
  virtual std::string GetWorkspace() const = 0;
  virtual bool IsVisibleOnAllWorkspaces() const = 0;

 protected:
  friend class BrowserCloseManager;
  friend class BrowserView;
  virtual void DestroyBrowser() = 0;

#if defined(OS_MACOSX)
  // Creates a Cocoa browser window, in browser builds where both Views and
  // Cocoa browsers windows are present.
  static BrowserWindow* CreateBrowserWindowCocoa(Browser* browser,
                                                 bool user_gesture);
#endif
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_H_
