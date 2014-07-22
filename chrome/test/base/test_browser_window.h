// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_
#define CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "chrome/browser/download/test_download_shelf.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"

class LocationBarTesting;
class OmniboxView;

namespace extensions {
class Extension;
}

// An implementation of BrowserWindow used for testing. TestBrowserWindow only
// contains a valid LocationBar, all other getters return NULL.
// See BrowserWithTestWindowTest for an example of using this class.
class TestBrowserWindow : public BrowserWindow {
 public:
  TestBrowserWindow();
  virtual ~TestBrowserWindow();

  // BrowserWindow:
  virtual void Show() OVERRIDE {}
  virtual void ShowInactive() OVERRIDE {}
  virtual void Hide() OVERRIDE {}
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE {}
  virtual void Close() OVERRIDE {}
  virtual void Activate() OVERRIDE {}
  virtual void Deactivate() OVERRIDE {}
  virtual bool IsActive() const OVERRIDE;
  virtual void FlashFrame(bool flash) OVERRIDE {}
  virtual bool IsAlwaysOnTop() const OVERRIDE;
  virtual void SetAlwaysOnTop(bool always_on_top) OVERRIDE {}
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;
  virtual BrowserWindowTesting* GetBrowserWindowTesting() OVERRIDE;
  virtual StatusBubble* GetStatusBubble() OVERRIDE;
  virtual void UpdateTitleBar() OVERRIDE {}
  virtual void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) OVERRIDE {}
  virtual void UpdateDevTools() OVERRIDE {}
  virtual void UpdateLoadingAnimations(bool should_animate) OVERRIDE {}
  virtual void SetStarredState(bool is_starred) OVERRIDE {}
  virtual void SetTranslateIconToggled(bool is_lit) OVERRIDE {}
  virtual void OnActiveTabChanged(content::WebContents* old_contents,
                                  content::WebContents* new_contents,
                                  int index,
                                  int reason) OVERRIDE {}
  virtual void ZoomChangedForActiveTab(bool can_show_bubble) OVERRIDE {}
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual ui::WindowShowState GetRestoredState() const OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual void Maximize() OVERRIDE {}
  virtual void Minimize() OVERRIDE {}
  virtual void Restore() OVERRIDE {}
  virtual void EnterFullscreen(
      const GURL& url, FullscreenExitBubbleType type) OVERRIDE {}
  virtual void ExitFullscreen() OVERRIDE {}
  virtual void UpdateFullscreenExitBubbleContent(
      const GURL& url,
      FullscreenExitBubbleType bubble_type) OVERRIDE {}
  virtual bool ShouldHideUIForFullscreen() const OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
#if defined(OS_WIN)
  virtual void SetMetroSnapMode(bool enable) OVERRIDE {}
  virtual bool IsInMetroSnapMode() const;
#endif
  virtual bool IsFullscreenBubbleVisible() const OVERRIDE;
  virtual LocationBar* GetLocationBar() const OVERRIDE;
  virtual void SetFocusToLocationBar(bool select_all) OVERRIDE {}
  virtual void UpdateReloadStopState(bool is_loading, bool force) OVERRIDE {}
  virtual void UpdateToolbar(content::WebContents* contents) OVERRIDE {}
  virtual void FocusToolbar() OVERRIDE {}
  virtual void FocusAppMenu() OVERRIDE {}
  virtual void FocusBookmarksToolbar() OVERRIDE {}
  virtual void FocusInfobars() OVERRIDE {}
  virtual void RotatePaneFocus(bool forwards) OVERRIDE {}
  virtual void ShowAppMenu() OVERRIDE {}
  virtual bool PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE {}

  virtual bool IsBookmarkBarVisible() const OVERRIDE;
  virtual bool IsBookmarkBarAnimating() const OVERRIDE;
  virtual bool IsTabStripEditable() const OVERRIDE;
  virtual bool IsToolbarVisible() const OVERRIDE;
  virtual gfx::Rect GetRootWindowResizerRect() const OVERRIDE;
  virtual void ConfirmAddSearchProvider(TemplateURL* template_url,
                                        Profile* profile) OVERRIDE {}
  virtual void ShowUpdateChromeDialog() OVERRIDE {}
  virtual void ShowBookmarkBubble(const GURL& url,
                                  bool already_bookmarked) OVERRIDE {}
  virtual void ShowBookmarkAppBubble(
      const WebApplicationInfo& web_app_info,
      const std::string& extension_id) OVERRIDE {}
  virtual void ShowTranslateBubble(
      content::WebContents* contents,
      translate::TranslateStep step,
      translate::TranslateErrors::Type error_type) OVERRIDE {}
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  virtual void ShowOneClickSigninBubble(
      OneClickSigninBubbleType type,
      const base::string16& email,
      const base::string16& error_message,
      const StartSyncCallback& start_sync_callback) OVERRIDE {}
#endif
  virtual bool IsDownloadShelfVisible() const OVERRIDE;
  virtual DownloadShelf* GetDownloadShelf() OVERRIDE;
  virtual void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadClosePreventionType dialog_type,
      bool app_modal,
      const base::Callback<void(bool)>& callback) OVERRIDE {}
  virtual void UserChangedTheme() OVERRIDE {}
  virtual int GetExtraRenderViewHeight() const OVERRIDE;
  virtual void WebContentsFocused(content::WebContents* contents) OVERRIDE {}
  virtual void ShowWebsiteSettings(Profile* profile,
                                   content::WebContents* web_contents,
                                   const GURL& url,
                                   const content::SSLStatus& ssl) OVERRIDE {}
  virtual void Cut() OVERRIDE {}
  virtual void Copy() OVERRIDE {}
  virtual void Paste() OVERRIDE {}
#if defined(OS_MACOSX)
  virtual void EnterFullscreenWithChrome() OVERRIDE {}
  virtual bool IsFullscreenWithChrome() OVERRIDE;
  virtual bool IsFullscreenWithoutChrome() OVERRIDE;
#endif

  virtual WindowOpenDisposition GetDispositionForPopupBounds(
      const gfx::Rect& bounds) OVERRIDE;
  virtual FindBar* CreateFindBar() OVERRIDE;
  virtual web_modal::WebContentsModalDialogHost*
      GetWebContentsModalDialogHost() OVERRIDE;
  virtual void ShowAvatarBubble(content::WebContents* web_contents,
                                const gfx::Rect& rect) OVERRIDE {}
  virtual void ShowAvatarBubbleFromAvatarButton(AvatarBubbleMode mode,
      const signin::ManageAccountsParams& manage_accounts_params) OVERRIDE {}
  virtual void ShowPasswordGenerationBubble(
      const gfx::Rect& rect,
      const autofill::PasswordForm& form,
      autofill::PasswordGenerator* generator) OVERRIDE {}
  virtual int GetRenderViewHeightInsetWithDetachedBookmarkBar() OVERRIDE;
  virtual void ExecuteExtensionCommand(
      const extensions::Extension* extension,
      const extensions::Command& command) OVERRIDE;
  virtual void ShowPageActionPopup(
      const extensions::Extension* extension) OVERRIDE;
  virtual void ShowBrowserActionPopup(
      const extensions::Extension* extension) OVERRIDE;

 protected:
  virtual void DestroyBrowser() OVERRIDE {}

 private:
  class TestLocationBar : public LocationBar {
   public:
    TestLocationBar() : LocationBar(NULL) {}
    virtual ~TestLocationBar() {}

    // LocationBar:
    virtual void ShowFirstRunBubble() OVERRIDE {}
    virtual GURL GetDestinationURL() const OVERRIDE;
    virtual WindowOpenDisposition GetWindowOpenDisposition() const OVERRIDE;
    virtual content::PageTransition GetPageTransition() const OVERRIDE;
    virtual void AcceptInput() OVERRIDE {}
    virtual void FocusLocation(bool select_all) OVERRIDE {}
    virtual void FocusSearch() OVERRIDE {}
    virtual void UpdateContentSettingsIcons() OVERRIDE {}
    virtual void UpdateManagePasswordsIconAndBubble() OVERRIDE {}
    virtual void UpdatePageActions() OVERRIDE {}
    virtual void InvalidatePageActions() OVERRIDE {}
    virtual void UpdateOpenPDFInReaderPrompt() OVERRIDE {}
    virtual void UpdateGeneratedCreditCardView() OVERRIDE {}
    virtual void SaveStateToContents(content::WebContents* contents) OVERRIDE {}
    virtual void Revert() OVERRIDE {}
    virtual const OmniboxView* GetOmniboxView() const OVERRIDE;
    virtual OmniboxView* GetOmniboxView() OVERRIDE;
    virtual LocationBarTesting* GetLocationBarForTesting() OVERRIDE;

   private:
    DISALLOW_COPY_AND_ASSIGN(TestLocationBar);
  };

  TestDownloadShelf download_shelf_;
  TestLocationBar location_bar_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserWindow);
};

namespace chrome {

// Helper that handle the lifetime of TestBrowserWindow instances.
Browser* CreateBrowserWithTestWindowForParams(Browser::CreateParams* params);

}  // namespace chrome

#endif  // CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_
