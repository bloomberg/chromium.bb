// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_COCOA_H_

#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/signin/signin_header_helper.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "ui/base/ui_base_types.h"

class Browser;
@class BrowserWindowController;
@class FindBarCocoaController;
@class NSEvent;
@class NSMenu;
@class NSWindow;

namespace extensions {
class ActiveTabPermissionGranter;
class Command;
class Extension;
}

// An implementation of BrowserWindow for Cocoa. Bridges between C++ and
// the Cocoa NSWindow. Cross-platform code will interact with this object when
// it needs to manipulate the window.

class BrowserWindowCocoa :
    public BrowserWindow,
    public extensions::ExtensionKeybindingRegistry::Delegate,
    public SearchModelObserver {
 public:
  BrowserWindowCocoa(Browser* browser,
                     BrowserWindowController* controller);
  virtual ~BrowserWindowCocoa();

  // Overridden from BrowserWindow
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
      const GURL& url, FullscreenExitBubbleType type) OVERRIDE;
  virtual void ExitFullscreen() OVERRIDE;
  virtual void UpdateFullscreenExitBubbleContent(
      const GURL& url,
      FullscreenExitBubbleType bubble_type) OVERRIDE;
  virtual bool ShouldHideUIForFullscreen() const OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual bool IsFullscreenBubbleVisible() const OVERRIDE;
  virtual LocationBar* GetLocationBar() const OVERRIDE;
  virtual void SetFocusToLocationBar(bool select_all) OVERRIDE;
  virtual void UpdateReloadStopState(bool is_loading, bool force) OVERRIDE;
  virtual void UpdateToolbar(content::WebContents* contents) OVERRIDE;
  virtual void FocusToolbar() OVERRIDE;
  virtual void FocusAppMenu() OVERRIDE;
  virtual void FocusBookmarksToolbar() OVERRIDE;
  virtual void FocusInfobars() OVERRIDE;
  virtual void RotatePaneFocus(bool forwards) OVERRIDE;
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
      translate::TranslateErrors::Type error_type,
      bool is_user_gesture) OVERRIDE;
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  virtual void ShowOneClickSigninBubble(
      OneClickSigninBubbleType type,
      const base::string16& email,
      const base::string16& error_message,
      const StartSyncCallback& start_sync_callback) OVERRIDE;
#endif
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
  virtual void EnterFullscreenWithChrome() OVERRIDE;
  virtual bool IsFullscreenWithChrome() OVERRIDE;
  virtual bool IsFullscreenWithoutChrome() OVERRIDE;
  virtual WindowOpenDisposition GetDispositionForPopupBounds(
      const gfx::Rect& bounds) OVERRIDE;
  virtual FindBar* CreateFindBar() OVERRIDE;
  virtual web_modal::WebContentsModalDialogHost*
      GetWebContentsModalDialogHost() OVERRIDE;
  virtual void ShowAvatarBubble(content::WebContents* web_contents,
                                const gfx::Rect& rect) OVERRIDE;
  virtual void ShowAvatarBubbleFromAvatarButton(AvatarBubbleMode mode,
      const signin::ManageAccountsParams& manage_accounts_params) OVERRIDE;
  virtual int GetRenderViewHeightInsetWithDetachedBookmarkBar() OVERRIDE;
  virtual void ExecuteExtensionCommand(
      const extensions::Extension* extension,
      const extensions::Command& command) OVERRIDE;

  // Overridden from ExtensionKeybindingRegistry::Delegate:
  virtual extensions::ActiveTabPermissionGranter*
      GetActiveTabPermissionGranter() OVERRIDE;

  // Overridden from SearchModelObserver:
  virtual void ModelChanged(const SearchModel::State& old_state,
                            const SearchModel::State& new_state) OVERRIDE;

  // Adds the given FindBar cocoa controller to this browser window.
  void AddFindBar(FindBarCocoaController* find_bar_cocoa_controller);

  // Returns the cocoa-world BrowserWindowController
  BrowserWindowController* cocoa_controller() { return controller_; }

 protected:
  virtual void DestroyBrowser() OVERRIDE;

 private:
  NSWindow* window() const;  // Accessor for the (current) |NSWindow|.

  Browser* browser_;  // weak, owned by controller
  BrowserWindowController* controller_;  // weak, owns us
  base::scoped_nsobject<NSString> pending_window_title_;
  ui::WindowShowState initial_show_state_;
  NSInteger attention_request_id_;  // identifier from requestUserAttention
};

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_COCOA_H_
