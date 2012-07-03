// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_window.h"

#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/rect.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/panels/panel_browser_view.h"
#endif

using content::NativeWebKeyboardEvent;
using content::SSLStatus;
using content::WebContents;

PanelBrowserWindow::PanelBrowserWindow(Browser* browser, Panel* panel,
                                       NativePanel* native_panel)
    : browser_(browser),
      panel_(panel),
      native_panel_(native_panel) {
  browser_->tab_strip_model()->AddObserver(this);
}

PanelBrowserWindow::~PanelBrowserWindow() {
  browser_->tab_strip_model()->RemoveObserver(this);
  // Invoked by native panel destructor via panel destructor.
  // Do not access native_panel_ nor panel_ here.
}

// ----------------------------------------------
// BaseWindow interface - just delegates to Panel
// ----------------------------------------------
bool PanelBrowserWindow::IsActive() const {
  return panel_->IsActive();
}

bool PanelBrowserWindow::IsMaximized() const {
  return panel_->IsMaximized();
}

bool PanelBrowserWindow::IsMinimized() const {
  return panel_->IsMinimized();
}

bool PanelBrowserWindow::IsFullscreen() const {
  return panel_->IsFullscreen();
}

gfx::Rect PanelBrowserWindow::GetRestoredBounds() const {
  return panel_->GetRestoredBounds();
}

gfx::Rect PanelBrowserWindow::GetBounds() const {
  return panel_->GetBounds();
}

void PanelBrowserWindow::Show() {
  panel_->Show();
}

void PanelBrowserWindow::ShowInactive() {
  panel_->ShowInactive();
}

void PanelBrowserWindow::Close() {
  panel_->Close();
}

void PanelBrowserWindow::Activate() {
  panel_->Activate();
}

void PanelBrowserWindow::Deactivate() {
  panel_->Deactivate();
}

void PanelBrowserWindow::Maximize() {
  panel_->Maximize();
}

void PanelBrowserWindow::Minimize() {
  panel_->Minimize();
}

void PanelBrowserWindow::Restore() {
  panel_->Restore();
}

void PanelBrowserWindow::SetBounds(const gfx::Rect& bounds) {
  panel_->SetBounds(bounds);
}

void PanelBrowserWindow::SetDraggableRegion(SkRegion* region) {
  panel_->SetDraggableRegion(region);
}

void PanelBrowserWindow::FlashFrame(bool flash) {
  panel_->FlashFrame(flash);
}

bool PanelBrowserWindow::IsAlwaysOnTop() const {
  return panel_->IsAlwaysOnTop();
}

// ------------------------------------------------------------
// BrowserWindow interface - either does nothing, delegates to
// native panel, delegates to Panel (WebContents handling)
// or invoke logic that has nothing to do with panels (GetDownloadShelf).
// -----------------------------------------------------------
gfx::NativeWindow PanelBrowserWindow::GetNativeWindow() {
  return native_panel_->GetNativePanelHandle();
}

BrowserWindowTesting* PanelBrowserWindow::GetBrowserWindowTesting() {
  NOTIMPLEMENTED();
  return NULL;
}

StatusBubble* PanelBrowserWindow::GetStatusBubble() {
  // TODO(jennb): Implement. http://crbug.com/102723
  return NULL;
}

void PanelBrowserWindow::UpdateTitleBar() {
  native_panel_->UpdatePanelTitleBar();
}

void PanelBrowserWindow::BookmarkBarStateChanged(
    BookmarkBar::AnimateChangeType change_type) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::UpdateDevTools() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::SetDevToolsDockSide(DevToolsDockSide side) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::UpdateLoadingAnimations(bool should_animate) {
  native_panel_->UpdatePanelLoadingAnimations(should_animate);
}

void PanelBrowserWindow::SetStarredState(bool is_starred) {
  // Since panels are typically not bookmarked extension UI windows, they don't
  // have starred state.
}

void PanelBrowserWindow::SetZoomIconState(ZoomController::ZoomIconState state) {
  // Since panels don't have an Omnibox, they don't have a zoom icon.
}

void PanelBrowserWindow::SetZoomIconTooltipPercent(int zoom_percent) {
  // Since panels don't have an Omnibox, they don't have a zoom icon.
}

void PanelBrowserWindow::ShowZoomBubble(int zoom_percent) {
  // Since panels don't have an Omnibox, they don't have a zoom icon, so no
  // bubble will appear from it.
}

void PanelBrowserWindow::EnterFullscreen(
      const GURL& url, FullscreenExitBubbleType type) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::ExitFullscreen() {
  NOTIMPLEMENTED();
}

#if defined(OS_WIN)
void PanelBrowserWindow::SetMetroSnapMode(bool enable) {
  NOTIMPLEMENTED();
}

bool PanelBrowserWindow::IsInMetroSnapMode() const {
  NOTIMPLEMENTED();
  return false;
}
#endif

void PanelBrowserWindow::UpdateFullscreenExitBubbleContent(
      const GURL& url,
      FullscreenExitBubbleType bubble_type) {
  NOTIMPLEMENTED();
}

bool PanelBrowserWindow::IsFullscreenBubbleVisible() const {
  NOTIMPLEMENTED();
  return false;
}

LocationBar* PanelBrowserWindow::GetLocationBar() const {
#if defined(USE_AURA)
  // TODO(stevenjb): Remove this when Aura panels are implemented post R18.
  PanelBrowserView* panel_view = static_cast<PanelBrowserView*>(native_panel_);
  return panel_view->GetLocationBar();
#else
  // Panels do not have a location bar.
  return NULL;
#endif
}

void PanelBrowserWindow::SetFocusToLocationBar(bool select_all) {
#if defined(USE_AURA)
  // TODO(stevenjb): Remove this when Aura panels are implemented post R18.
  PanelBrowserView* panel_view = static_cast<PanelBrowserView*>(native_panel_);
  panel_view->SetFocusToLocationBar(select_all);
#else
  // Panels do not have a location bar.
#endif
}

void PanelBrowserWindow::UpdateReloadStopState(bool is_loading, bool force) {
  // Panels don't have stop/reload indicator.
}

void PanelBrowserWindow::UpdateToolbar(TabContents* contents,
                                       bool should_restore_state) {
  // Panels do not have a toolbar.
}

void PanelBrowserWindow::FocusToolbar() {
  // Panels do not have a toolbar.
}

void PanelBrowserWindow::FocusAppMenu() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::FocusBookmarksToolbar() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::RotatePaneFocus(bool forwards) {
  NOTIMPLEMENTED();
}

bool PanelBrowserWindow::IsBookmarkBarVisible() const {
  return false;
}

bool PanelBrowserWindow::IsBookmarkBarAnimating() const {
  return false;
}

// This is used by extensions to decide if a window can be closed.
// Always return true as panels do not have tabs that can be dragged,
// during which extensions will avoid closing a window.
bool PanelBrowserWindow::IsTabStripEditable() const {
  return true;
}

bool PanelBrowserWindow::IsToolbarVisible() const {
  NOTIMPLEMENTED();
  return false;
}

gfx::Rect PanelBrowserWindow::GetRootWindowResizerRect() const {
  return gfx::Rect();
}

bool PanelBrowserWindow::IsPanel() const {
  return true;
}

void PanelBrowserWindow::DisableInactiveFrame() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::ConfirmAddSearchProvider(TemplateURL* template_url,
                                                  Profile* profile) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::ToggleBookmarkBar() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::ShowAboutChromeDialog() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::ShowUpdateChromeDialog() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::ShowTaskManager() {
  native_panel_->ShowTaskManagerForPanel();
}

void PanelBrowserWindow::ShowBackgroundPages() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::ShowBookmarkBubble(const GURL& url,
                                            bool already_bookmarked) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::ShowChromeToMobileBubble() {
  NOTIMPLEMENTED();
}

#if defined(ENABLE_ONE_CLICK_SIGNIN)
void PanelBrowserWindow::ShowOneClickSigninBubble(
      const StartSyncCallback& start_sync_callback) {
  NOTIMPLEMENTED();
}
#endif

bool PanelBrowserWindow::IsDownloadShelfVisible() const {
  return false;
}

DownloadShelf* PanelBrowserWindow::GetDownloadShelf() {
  Profile* profile = browser_->profile();
  Browser* tabbed_browser = browser::FindTabbedBrowser(profile, true);

  if (!tabbed_browser) {
    // Set initial bounds so window will not be positioned at an offset
    // to this panel as panels are at the bottom of the screen.
    gfx::Rect window_bounds;
    WindowSizer::GetBrowserWindowBounds(std::string(), gfx::Rect(),
                                        browser_, &window_bounds);
    Browser::CreateParams params(Browser::TYPE_TABBED, profile);
    params.initial_bounds = window_bounds;
    tabbed_browser = Browser::CreateWithParams(params);
    chrome::NewTab(tabbed_browser);
  }

  tabbed_browser->window()->Show();  // Ensure download shelf is visible.
  return tabbed_browser->window()->GetDownloadShelf();
}

void PanelBrowserWindow::ConfirmBrowserCloseWithPendingDownloads() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::UserChangedTheme() {
  native_panel_->NotifyPanelOnUserChangedTheme();
}

int PanelBrowserWindow::GetExtraRenderViewHeight() const {
  // This is currently used only on Linux and that returns the height for
  // optional elements like bookmark bar, download bar etc.  Not applicable to
  // panels.
  return 0;
}

void PanelBrowserWindow::WebContentsFocused(WebContents* contents) {
  native_panel_->PanelWebContentsFocused(contents);
}

void PanelBrowserWindow::ShowPageInfo(WebContents* web_contents,
                                      const GURL& url,
                                      const SSLStatus& ssl,
                                      bool show_history) {
  NOTIMPLEMENTED();
}
void PanelBrowserWindow::ShowWebsiteSettings(
    Profile* profile,
    TabContents* tab_contents,
    const GURL& url,
    const content::SSLStatus& ssl,
    bool show_history) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::ShowAppMenu() {
  NOTIMPLEMENTED();
}

bool PanelBrowserWindow::PreHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return native_panel_->PreHandlePanelKeyboardEvent(event,
                                                    is_keyboard_shortcut);
}

void PanelBrowserWindow::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  native_panel_->HandlePanelKeyboardEvent(event);
}

void PanelBrowserWindow::ShowCreateChromeAppShortcutsDialog(
    Profile* profile,
    const extensions::Extension* app) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::Cut() {
  native_panel_->PanelCut();
}

void PanelBrowserWindow::Copy() {
  native_panel_->PanelCopy();
}

void PanelBrowserWindow::Paste() {
  native_panel_->PanelPaste();
}

#if defined(OS_MACOSX)
void PanelBrowserWindow::OpenTabpose() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::EnterPresentationMode(
      const GURL& url,
      FullscreenExitBubbleType content_type) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::ExitPresentationMode() {
  NOTIMPLEMENTED();
}

bool PanelBrowserWindow::InPresentationMode() {
  NOTIMPLEMENTED();
  return false;
}
#endif

void PanelBrowserWindow::ShowInstant(TabContents* preview) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindow::HideInstant() {
  NOTIMPLEMENTED();
}

gfx::Rect PanelBrowserWindow::GetInstantBounds() {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

WindowOpenDisposition PanelBrowserWindow::GetDispositionForPopupBounds(
    const gfx::Rect& bounds) {
#if defined(USE_AURA)
  // TODO(stevenjb): Remove this when Aura panels are implemented post R18.
  PanelBrowserView* panel_view = static_cast<PanelBrowserView*>(native_panel_);
  return panel_view->GetDispositionForPopupBounds(bounds);
#else
  return NEW_POPUP;
#endif
}

FindBar* PanelBrowserWindow::CreateFindBar() {
  return native_panel_->CreatePanelFindBar();
}

void PanelBrowserWindow::ResizeDueToAutoResize(WebContents* web_contents,
                                               const gfx::Size& pref_size) {
  DCHECK(panel_->auto_resizable());
  return panel_->OnContentsAutoResized(pref_size);
}

void PanelBrowserWindow::ShowAvatarBubble(WebContents* web_contents,
                                          const gfx::Rect& rect) {
  // Panels will never show a new tab page so this should never be called.
  NOTREACHED();
}

void PanelBrowserWindow::ShowAvatarBubbleFromAvatarButton() {
  // Panels will never show an avatar button so this should never be called.
  NOTREACHED();
}

void PanelBrowserWindow::DestroyBrowser() {
  native_panel_->DestroyPanelBrowser();
}

void PanelBrowserWindow::TabInsertedAt(TabContents* contents,
                                       int index,
                                       bool foreground) {
  if (panel_->auto_resizable()) {
    DCHECK_EQ(0, index);
    panel_->EnableWebContentsAutoResize(contents->web_contents());
  }
}
