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
  virtual void Show() override;
  virtual void ShowInactive() override;
  virtual void Hide() override;
  virtual void SetBounds(const gfx::Rect& bounds) override;
  virtual void Close() override;
  virtual void Activate() override;
  virtual void Deactivate() override;
  virtual bool IsActive() const override;
  virtual void FlashFrame(bool flash) override;
  virtual bool IsAlwaysOnTop() const override;
  virtual void SetAlwaysOnTop(bool always_on_top) override;
  virtual gfx::NativeWindow GetNativeWindow() override;
  virtual BrowserWindowTesting* GetBrowserWindowTesting() override;
  virtual StatusBubble* GetStatusBubble() override;
  virtual void UpdateTitleBar() override;
  virtual void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) override;
  virtual void UpdateDevTools() override;
  virtual void UpdateLoadingAnimations(bool should_animate) override;
  virtual void SetStarredState(bool is_starred) override;
  virtual void SetTranslateIconToggled(bool is_lit) override;
  virtual void OnActiveTabChanged(content::WebContents* old_contents,
                                  content::WebContents* new_contents,
                                  int index,
                                  int reason) override;
  virtual void ZoomChangedForActiveTab(bool can_show_bubble) override;
  virtual gfx::Rect GetRestoredBounds() const override;
  virtual ui::WindowShowState GetRestoredState() const override;
  virtual gfx::Rect GetBounds() const override;
  virtual bool IsMaximized() const override;
  virtual bool IsMinimized() const override;
  virtual void Maximize() override;
  virtual void Minimize() override;
  virtual void Restore() override;
  virtual void EnterFullscreen(
      const GURL& url, FullscreenExitBubbleType type) override;
  virtual void ExitFullscreen() override;
  virtual void UpdateFullscreenExitBubbleContent(
      const GURL& url,
      FullscreenExitBubbleType bubble_type) override;
  virtual bool ShouldHideUIForFullscreen() const override;
  virtual bool IsFullscreen() const override;
  virtual bool IsFullscreenBubbleVisible() const override;
  virtual LocationBar* GetLocationBar() const override;
  virtual void SetFocusToLocationBar(bool select_all) override;
  virtual void UpdateReloadStopState(bool is_loading, bool force) override;
  virtual void UpdateToolbar(content::WebContents* contents) override;
  virtual void FocusToolbar() override;
  virtual void FocusAppMenu() override;
  virtual void FocusBookmarksToolbar() override;
  virtual void FocusInfobars() override;
  virtual void RotatePaneFocus(bool forwards) override;
  virtual bool IsBookmarkBarVisible() const override;
  virtual bool IsBookmarkBarAnimating() const override;
  virtual bool IsTabStripEditable() const override;
  virtual bool IsToolbarVisible() const override;
  virtual gfx::Rect GetRootWindowResizerRect() const override;
  virtual void ConfirmAddSearchProvider(TemplateURL* template_url,
                                        Profile* profile) override;
  virtual void ShowUpdateChromeDialog() override;
  virtual void ShowBookmarkBubble(const GURL& url,
                                  bool already_bookmarked) override;
  virtual void ShowBookmarkAppBubble(
      const WebApplicationInfo& web_app_info,
      const std::string& extension_id) override;
  virtual void ShowTranslateBubble(
      content::WebContents* contents,
      translate::TranslateStep step,
      translate::TranslateErrors::Type error_type,
      bool is_user_gesture) override;
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  virtual void ShowOneClickSigninBubble(
      OneClickSigninBubbleType type,
      const base::string16& email,
      const base::string16& error_message,
      const StartSyncCallback& start_sync_callback) override;
#endif
  virtual bool IsDownloadShelfVisible() const override;
  virtual DownloadShelf* GetDownloadShelf() override;
  virtual void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadClosePreventionType dialog_type,
      bool app_modal,
      const base::Callback<void(bool)>& callback) override;
  virtual void UserChangedTheme() override;
  virtual int GetExtraRenderViewHeight() const override;
  virtual void WebContentsFocused(content::WebContents* contents) override;
  virtual void ShowWebsiteSettings(Profile* profile,
                                   content::WebContents* web_contents,
                                   const GURL& url,
                                   const content::SSLStatus& ssl) override;
  virtual void ShowAppMenu() override;
  virtual bool PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) override;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  virtual void Cut() override;
  virtual void Copy() override;
  virtual void Paste() override;
  virtual void EnterFullscreenWithChrome() override;
  virtual bool IsFullscreenWithChrome() override;
  virtual bool IsFullscreenWithoutChrome() override;
  virtual WindowOpenDisposition GetDispositionForPopupBounds(
      const gfx::Rect& bounds) override;
  virtual FindBar* CreateFindBar() override;
  virtual web_modal::WebContentsModalDialogHost*
      GetWebContentsModalDialogHost() override;
  virtual void ShowAvatarBubble(content::WebContents* web_contents,
                                const gfx::Rect& rect) override;
  virtual void ShowAvatarBubbleFromAvatarButton(AvatarBubbleMode mode,
      const signin::ManageAccountsParams& manage_accounts_params) override;
  virtual int GetRenderViewHeightInsetWithDetachedBookmarkBar() override;
  virtual void ExecuteExtensionCommand(
      const extensions::Extension* extension,
      const extensions::Command& command) override;

  // Overridden from ExtensionKeybindingRegistry::Delegate:
  virtual extensions::ActiveTabPermissionGranter*
      GetActiveTabPermissionGranter() override;

  // Overridden from SearchModelObserver:
  virtual void ModelChanged(const SearchModel::State& old_state,
                            const SearchModel::State& new_state) override;

  // Adds the given FindBar cocoa controller to this browser window.
  void AddFindBar(FindBarCocoaController* find_bar_cocoa_controller);

  // Returns the cocoa-world BrowserWindowController
  BrowserWindowController* cocoa_controller() { return controller_; }

 protected:
  virtual void DestroyBrowser() override;

 private:
  NSWindow* window() const;  // Accessor for the (current) |NSWindow|.

  Browser* browser_;  // weak, owned by controller
  BrowserWindowController* controller_;  // weak, owns us
  base::scoped_nsobject<NSString> pending_window_title_;
  ui::WindowShowState initial_show_state_;
  NSInteger attention_request_id_;  // identifier from requestUserAttention
};

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_COCOA_H_
