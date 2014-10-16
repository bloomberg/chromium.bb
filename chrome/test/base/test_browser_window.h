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
  virtual void Show() override {}
  virtual void ShowInactive() override {}
  virtual void Hide() override {}
  virtual void SetBounds(const gfx::Rect& bounds) override {}
  virtual void Close() override {}
  virtual void Activate() override {}
  virtual void Deactivate() override {}
  virtual bool IsActive() const override;
  virtual void FlashFrame(bool flash) override {}
  virtual bool IsAlwaysOnTop() const override;
  virtual void SetAlwaysOnTop(bool always_on_top) override {}
  virtual gfx::NativeWindow GetNativeWindow() override;
  virtual BrowserWindowTesting* GetBrowserWindowTesting() override;
  virtual StatusBubble* GetStatusBubble() override;
  virtual void UpdateTitleBar() override {}
  virtual void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) override {}
  virtual void UpdateDevTools() override {}
  virtual void UpdateLoadingAnimations(bool should_animate) override {}
  virtual void SetStarredState(bool is_starred) override {}
  virtual void SetTranslateIconToggled(bool is_lit) override {}
  virtual void OnActiveTabChanged(content::WebContents* old_contents,
                                  content::WebContents* new_contents,
                                  int index,
                                  int reason) override {}
  virtual void ZoomChangedForActiveTab(bool can_show_bubble) override {}
  virtual gfx::Rect GetRestoredBounds() const override;
  virtual ui::WindowShowState GetRestoredState() const override;
  virtual gfx::Rect GetBounds() const override;
  virtual bool IsMaximized() const override;
  virtual bool IsMinimized() const override;
  virtual void Maximize() override {}
  virtual void Minimize() override {}
  virtual void Restore() override {}
  virtual void EnterFullscreen(
      const GURL& url, FullscreenExitBubbleType type) override {}
  virtual void ExitFullscreen() override {}
  virtual void UpdateFullscreenExitBubbleContent(
      const GURL& url,
      FullscreenExitBubbleType bubble_type) override {}
  virtual bool ShouldHideUIForFullscreen() const override;
  virtual bool IsFullscreen() const override;
#if defined(OS_WIN)
  virtual void SetMetroSnapMode(bool enable) override {}
  virtual bool IsInMetroSnapMode() const;
#endif
  virtual bool IsFullscreenBubbleVisible() const override;
  virtual LocationBar* GetLocationBar() const override;
  virtual void SetFocusToLocationBar(bool select_all) override {}
  virtual void UpdateReloadStopState(bool is_loading, bool force) override {}
  virtual void UpdateToolbar(content::WebContents* contents) override {}
  virtual void FocusToolbar() override {}
  virtual void FocusAppMenu() override {}
  virtual void FocusBookmarksToolbar() override {}
  virtual void FocusInfobars() override {}
  virtual void RotatePaneFocus(bool forwards) override {}
  virtual void ShowAppMenu() override {}
  virtual bool PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) override;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override {}

  virtual bool IsBookmarkBarVisible() const override;
  virtual bool IsBookmarkBarAnimating() const override;
  virtual bool IsTabStripEditable() const override;
  virtual bool IsToolbarVisible() const override;
  virtual gfx::Rect GetRootWindowResizerRect() const override;
  virtual void ConfirmAddSearchProvider(TemplateURL* template_url,
                                        Profile* profile) override {}
  virtual void ShowUpdateChromeDialog() override {}
  virtual void ShowBookmarkBubble(const GURL& url,
                                  bool already_bookmarked) override {}
  virtual void ShowBookmarkAppBubble(
      const WebApplicationInfo& web_app_info,
      const std::string& extension_id) override {}
  virtual void ShowTranslateBubble(
      content::WebContents* contents,
      translate::TranslateStep step,
      translate::TranslateErrors::Type error_type,
      bool is_user_gesture) override {}
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  virtual void ShowOneClickSigninBubble(
      OneClickSigninBubbleType type,
      const base::string16& email,
      const base::string16& error_message,
      const StartSyncCallback& start_sync_callback) override {}
#endif
  virtual bool IsDownloadShelfVisible() const override;
  virtual DownloadShelf* GetDownloadShelf() override;
  virtual void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadClosePreventionType dialog_type,
      bool app_modal,
      const base::Callback<void(bool)>& callback) override {}
  virtual void UserChangedTheme() override {}
  virtual int GetExtraRenderViewHeight() const override;
  virtual void WebContentsFocused(content::WebContents* contents) override {}
  virtual void ShowWebsiteSettings(Profile* profile,
                                   content::WebContents* web_contents,
                                   const GURL& url,
                                   const content::SSLStatus& ssl) override {}
  virtual void Cut() override {}
  virtual void Copy() override {}
  virtual void Paste() override {}
#if defined(OS_MACOSX)
  virtual void EnterFullscreenWithChrome() override {}
  virtual void EnterFullscreenWithoutChrome() override {}
  virtual bool IsFullscreenWithChrome() override;
  virtual bool IsFullscreenWithoutChrome() override;
#endif

  virtual WindowOpenDisposition GetDispositionForPopupBounds(
      const gfx::Rect& bounds) override;
  virtual FindBar* CreateFindBar() override;
  virtual web_modal::WebContentsModalDialogHost*
      GetWebContentsModalDialogHost() override;
  virtual void ShowAvatarBubble(content::WebContents* web_contents,
                                const gfx::Rect& rect) override {}
  virtual void ShowAvatarBubbleFromAvatarButton(AvatarBubbleMode mode,
      const signin::ManageAccountsParams& manage_accounts_params) override {}
  virtual int GetRenderViewHeightInsetWithDetachedBookmarkBar() override;
  virtual void ExecuteExtensionCommand(
      const extensions::Extension* extension,
      const extensions::Command& command) override;

 protected:
  virtual void DestroyBrowser() override {}

 private:
  class TestLocationBar : public LocationBar {
   public:
    TestLocationBar() : LocationBar(NULL) {}
    virtual ~TestLocationBar() {}

    // LocationBar:
    virtual void ShowFirstRunBubble() override {}
    virtual GURL GetDestinationURL() const override;
    virtual WindowOpenDisposition GetWindowOpenDisposition() const override;
    virtual ui::PageTransition GetPageTransition() const override;
    virtual void AcceptInput() override {}
    virtual void FocusLocation(bool select_all) override {}
    virtual void FocusSearch() override {}
    virtual void UpdateContentSettingsIcons() override {}
    virtual void UpdateManagePasswordsIconAndBubble() override {}
    virtual void UpdatePageActions() override {}
    virtual void InvalidatePageActions() override {}
    virtual void UpdateBookmarkStarVisibility() override {}
    virtual bool ShowPageActionPopup(const extensions::Extension* extension,
                                     bool grant_active_tab) override;
    virtual void UpdateOpenPDFInReaderPrompt() override {}
    virtual void UpdateGeneratedCreditCardView() override {}
    virtual void SaveStateToContents(content::WebContents* contents) override {}
    virtual void Revert() override {}
    virtual const OmniboxView* GetOmniboxView() const override;
    virtual OmniboxView* GetOmniboxView() override;
    virtual LocationBarTesting* GetLocationBarForTesting() override;

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
