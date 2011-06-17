// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_window_cocoa.h"

#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/panels/panel.h"
#import "chrome/browser/ui/panels/panel_window_controller_cocoa.h"

// This creates a shim window class, which in turn creates a Cocoa window
// controller which in turn creates actual NSWindow by loading a nib.
// Panel contains this class as a nested BrowserWindow, but the overall
// chain of ownership is:
// PanelWindowControllerCocoa -> PanelBrowserWindowCocoa -> Panel.
NativePanel* Panel::CreateNativePanel(Browser* browser, Panel* panel) {
  return new PanelBrowserWindowCocoa(browser, panel);
}

PanelBrowserWindowCocoa::PanelBrowserWindowCocoa(Browser* browser,
                                                 Panel* panel)
  : browser_(browser),
    panel_(panel) {
  controller_ = [[PanelWindowControllerCocoa alloc] initWithBrowserWindow:this];
}

PanelBrowserWindowCocoa::~PanelBrowserWindowCocoa() {
}

bool PanelBrowserWindowCocoa::isClosed() {
  return !controller_;
}

void PanelBrowserWindowCocoa::ShowPanel() {
  if (isClosed())
    return;

  NSRect finalFrame = ConvertCoordinatesToCocoa(panel_->GetBounds());
  NSRect startFrame = NSMakeRect(NSMinX(finalFrame), NSMinY(finalFrame),
                                 NSWidth(finalFrame), 0);
  // Show the window, using OS-specific animation.
  [nswindow() setFrame:startFrame display:NO animate:NO];
  [controller_ showWindow:nil];
  [nswindow() setFrame:finalFrame display:YES animate:YES];
}

void PanelBrowserWindowCocoa::SetPanelBounds(const gfx::Rect& bounds) {
  NSRect frame = ConvertCoordinatesToCocoa(bounds);
  [nswindow() setFrame:frame display:YES animate:YES];
}

void PanelBrowserWindowCocoa::MinimizePanel() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::RestorePanel() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ClosePanel() {
  if (isClosed())
      return;

  NSRect frame = [nswindow() frame];
  frame.size.height = 0;
  [nswindow() setFrame:frame display:YES animate:YES];
  browser_->OnWindowClosing();
  DestroyBrowser();  // not immediately, though.
}

void PanelBrowserWindowCocoa::ActivatePanel() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::DeactivatePanel() {
  NOTIMPLEMENTED();
}

bool PanelBrowserWindowCocoa::IsPanelActive() const {
  NOTIMPLEMENTED();
  return false;
}

gfx::NativeWindow PanelBrowserWindowCocoa::GetNativePanelHandle() {
  return nswindow();
}

void PanelBrowserWindowCocoa::UpdatePanelTitleBar() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ShowTaskManagerForPanel() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::NotifyPanelOnUserChangedTheme() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::FlashPanelFrame() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::DestroyPanelBrowser() {
  [controller_ close];
  [controller_ autorelease];
  controller_ = NULL;
}

void PanelBrowserWindowCocoa::Show() {
  ShowPanel();  // Delegate to NativePanel.
}

void PanelBrowserWindowCocoa::ShowInactive() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::SetBounds(const gfx::Rect& bounds) {
  SetPanelBounds(bounds);  // Delegate to NativePanel.
}

// Callers assume that this doesn't immediately delete the Browser object.
void PanelBrowserWindowCocoa::Close() {
  ClosePanel();  // Delegate to NativePanel.
}

void PanelBrowserWindowCocoa::Activate() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::Deactivate() {
  NOTIMPLEMENTED();
}

bool PanelBrowserWindowCocoa::IsActive() const {
  NOTIMPLEMENTED();
  return false;
}

void PanelBrowserWindowCocoa::FlashFrame() {
  NOTIMPLEMENTED();
}

gfx::NativeWindow PanelBrowserWindowCocoa::GetNativeHandle() {
  return GetNativePanelHandle();  // Delegate to NativePanel.
}

BrowserWindowTesting* PanelBrowserWindowCocoa::GetBrowserWindowTesting() {
  NOTIMPLEMENTED();
  return NULL;
}

StatusBubble* PanelBrowserWindowCocoa::GetStatusBubble() {
  NOTIMPLEMENTED();
  return NULL;
}

void PanelBrowserWindowCocoa::ToolbarSizeChanged(bool is_animating){
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::UpdateTitleBar() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ShelfVisibilityChanged() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::UpdateDevTools() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::UpdateLoadingAnimations(bool should_animate) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::SetStarredState(bool is_starred) {
  NOTIMPLEMENTED();
}

gfx::Rect PanelBrowserWindowCocoa::GetRestoredBounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

gfx::Rect PanelBrowserWindowCocoa::GetBounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

bool PanelBrowserWindowCocoa::IsMaximized() const {
  NOTIMPLEMENTED();
  return false;
}

void PanelBrowserWindowCocoa::SetFullscreen(bool fullscreen) {
  NOTIMPLEMENTED();
}

bool PanelBrowserWindowCocoa::IsFullscreen() const {
  NOTIMPLEMENTED();
  return false;
}

bool PanelBrowserWindowCocoa::IsFullscreenBubbleVisible() const {
  NOTIMPLEMENTED();
  return false;
}

LocationBar* PanelBrowserWindowCocoa::GetLocationBar() const {
  NOTIMPLEMENTED();
  return NULL;
}

void PanelBrowserWindowCocoa::SetFocusToLocationBar(bool select_all) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::UpdateReloadStopState(bool is_loading,
                                                    bool force) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::UpdateToolbar(TabContentsWrapper* contents,
                                            bool should_restore_state) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::FocusToolbar() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::FocusAppMenu() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::FocusBookmarksToolbar() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::FocusChromeOSStatus() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::RotatePaneFocus(bool forwards) {
  NOTIMPLEMENTED();
}

bool PanelBrowserWindowCocoa::IsBookmarkBarVisible() const {
  return false;
}

bool PanelBrowserWindowCocoa::IsBookmarkBarAnimating() const {
  return false;
}

bool PanelBrowserWindowCocoa::IsTabStripEditable() const {
  return false;
}

bool PanelBrowserWindowCocoa::IsToolbarVisible() const {
  NOTIMPLEMENTED();
  return false;
}

void PanelBrowserWindowCocoa::ConfirmAddSearchProvider(
    const TemplateURL* template_url,
    Profile* profile) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ToggleBookmarkBar() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ShowAboutChromeDialog() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ShowUpdateChromeDialog() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ShowTaskManager() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ShowBackgroundPages() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ShowBookmarkBubble(const GURL& url,
                                                 bool already_bookmarked) {
  NOTIMPLEMENTED();
}

bool PanelBrowserWindowCocoa::IsDownloadShelfVisible() const {
  NOTIMPLEMENTED();
  return false;
}

DownloadShelf* PanelBrowserWindowCocoa::GetDownloadShelf() {
  NOTIMPLEMENTED();
  return NULL;
}

void PanelBrowserWindowCocoa::ShowRepostFormWarningDialog(
    TabContents* tab_contents) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ShowCollectedCookiesDialog(
    TabContents* tab_contents) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ShowThemeInstallBubble() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ConfirmBrowserCloseWithPendingDownloads() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
                                             gfx::NativeWindow parent_window) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::UserChangedTheme() {
  NOTIMPLEMENTED();
}

int PanelBrowserWindowCocoa::GetExtraRenderViewHeight() const {
  NOTIMPLEMENTED();
  return -1;
}

void PanelBrowserWindowCocoa::TabContentsFocused(TabContents* tab_contents) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ShowPageInfo(
    Profile* profile,
    const GURL& url,
    const NavigationEntry::SSLStatus& ssl,
    bool show_history) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ShowAppMenu() {
  NOTIMPLEMENTED();
}

bool PanelBrowserWindowCocoa::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  NOTIMPLEMENTED();
  return false;
}

void PanelBrowserWindowCocoa::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ShowCreateWebAppShortcutsDialog(
    TabContentsWrapper* tab_contents) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ShowCreateChromeAppShortcutsDialog(
    Profile* profile,
    const Extension* app) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::Cut() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::Copy() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::Paste() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ToggleTabStripMode() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ToggleUseCompactNavigationBar() {
  NOTIMPLEMENTED();
}

#if defined(OS_MACOSX)
void PanelBrowserWindowCocoa::OpenTabpose() {
  NOTIMPLEMENTED();
}
#endif

void PanelBrowserWindowCocoa::PrepareForInstant() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ShowInstant(TabContentsWrapper* preview) {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::HideInstant(bool instant_is_active) {
  NOTIMPLEMENTED();
}

gfx::Rect PanelBrowserWindowCocoa::GetInstantBounds() {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

WindowOpenDisposition PanelBrowserWindowCocoa::GetDispositionForPopupBounds(
      const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
  return NEW_POPUP;
}

void PanelBrowserWindowCocoa::DestroyBrowser() {
  DestroyPanelBrowser();  // Delegate to NativePanel.
}

NSWindow* PanelBrowserWindowCocoa::nswindow() const {
  return [controller_ window];
}

NSRect PanelBrowserWindowCocoa::ConvertCoordinatesToCocoa(
    const gfx::Rect& bounds) {
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];

  return NSMakeRect(
      bounds.x(), NSHeight([screen frame]) - bounds.height() - bounds.y(),
      bounds.width(), bounds.height());
}

