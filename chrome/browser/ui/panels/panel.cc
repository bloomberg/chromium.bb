// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel.h"

#include "base/logging.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/window_sizer.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/content_notification_types.h"
#include "content/common/view_messages.h"
#include "ui/gfx/rect.h"

// static
const Extension* Panel::GetExtensionFromBrowser(Browser* browser) {
  // Find the extension. When we create a panel from an extension, the extension
  // ID is passed as the app name to the Browser.
  ExtensionService* extension_service =
      browser->GetProfile()->GetExtensionService();
  return extension_service->GetExtensionById(
      web_app::GetExtensionIdFromApplicationName(browser->app_name()), false);
}

Panel::Panel(Browser* browser, const gfx::Rect& bounds)
    : min_size_(bounds.size()),
      max_size_(bounds.size()),
      native_panel_(NULL),
      expansion_state_(EXPANDED) {
  native_panel_ = CreateNativePanel(browser, this, bounds);

  registrar_.Add(this,
                 content::NOTIFICATION_TAB_ADDED,
                 Source<TabContentsDelegate>(browser));
}

Panel::~Panel() {
  // Invoked by native panel so do not access native_panel_ here.
}

PanelManager* Panel::manager() const {
  return PanelManager::GetInstance();
}


const Extension* Panel::GetExtension() const {
  return GetExtensionFromBrowser(browser());
}

void Panel::SetPanelBounds(const gfx::Rect& bounds) {
  native_panel_->SetPanelBounds(bounds);
}

void Panel::SetMaxSize(const gfx::Size& max_size) {
  if (max_size_ == max_size)
    return;
  max_size_ = max_size;

  // Note: |render_view_host| might be NULL if the tab has not been created.
  // If so, we will do it when NOTIFICATION_TAB_ADDED is received.
  RenderViewHost* render_view_host = GetRenderViewHost();
  if (render_view_host)
    RequestRenderViewHostToDisableScrollbars(render_view_host);
}

void Panel::SetExpansionState(ExpansionState new_expansion_state) {
  if (expansion_state_ == new_expansion_state)
    return;

  expansion_state_ = new_expansion_state;

  native_panel_->OnPanelExpansionStateChanged(expansion_state_);

  // The minimized panel should not get the focus.
  if (expansion_state_ == MINIMIZED)
    Deactivate();
}

bool Panel::ShouldBringUpTitlebar(int mouse_x, int mouse_y) const {
  // Skip the expanded panel.
  if (expansion_state_ == Panel::EXPANDED)
    return false;

  // If the panel is showing titlebar only, we want to keep it up when it is
  // being dragged.
  if (expansion_state_ == Panel::TITLE_ONLY && manager()->is_dragging_panel())
    return true;

  // Let the native panel decide.
  return native_panel_->ShouldBringUpPanelTitlebar(mouse_x, mouse_y);
}

bool Panel::IsDrawingAttention() const {
  return native_panel_->IsDrawingAttention();
}

int Panel::GetRestoredHeight() const {
  return native_panel_->GetRestoredHeight();
}

void Panel::SetRestoredHeight(int height) {
  native_panel_->SetRestoredHeight(height);
}

void Panel::Show() {
  native_panel_->ShowPanel();
}

void Panel::ShowInactive() {
  native_panel_->ShowPanelInactive();
}

void Panel::SetBounds(const gfx::Rect& bounds) {
  // Ignore any SetBounds requests since the bounds are completely controlled
  // by panel manager.
}

// Close() may be called multiple times if the browser window is not ready to
// close on the first attempt.
void Panel::Close() {
  native_panel_->ClosePanel();

// TODO(dimich): Only implemented fully async on Mac. Need to update other
// platforms. The panel should be removed from PanelManager when and if it
// was actually closed. The closing can be cancelled because of onbeforeunload
// handler on the web page.
#if !defined(OS_MACOSX)
  manager()->Remove(this);
#endif
}

void Panel::Activate() {
  native_panel_->ActivatePanel();
}

void Panel::Deactivate() {
  native_panel_->DeactivatePanel();
}

bool Panel::IsActive() const {
  return native_panel_->IsPanelActive();
}

void Panel::FlashFrame() {
  native_panel_->DrawAttention();
}

gfx::NativeWindow Panel::GetNativeHandle() {
  return native_panel_->GetNativePanelHandle();
}

BrowserWindowTesting* Panel::GetBrowserWindowTesting() {
  NOTIMPLEMENTED();
  return NULL;
}

StatusBubble* Panel::GetStatusBubble() {
  // TODO(jennb): Implement.
  return NULL;
}

void Panel::ToolbarSizeChanged(bool is_animating){
  NOTIMPLEMENTED();
}

void Panel::UpdateTitleBar() {
  native_panel_->UpdatePanelTitleBar();
}

void Panel::BookmarkBarStateChanged(
    BookmarkBar::AnimateChangeType change_type) {
  NOTIMPLEMENTED();
}

void Panel::UpdateDevTools() {
  NOTIMPLEMENTED();
}

void Panel::UpdateLoadingAnimations(bool should_animate) {
  // TODO(jennb): Implement.
}

void Panel::SetStarredState(bool is_starred) {
  // TODO(jennb): Figure out if this applies to Panels.
}

gfx::Rect Panel::GetRestoredBounds() const {
  return native_panel_->GetPanelBounds();
}

gfx::Rect Panel::GetBounds() const {
  return native_panel_->GetPanelBounds();
}

bool Panel::IsMaximized() const {
  // TODO(jennb): Figure out how this applies to Panels. Means isZoomed.
  return false;
}

bool Panel::IsMinimized() const {
  NOTIMPLEMENTED();
  return false;
}

void Panel::SetFullscreen(bool fullscreen) {
  NOTIMPLEMENTED();
}

bool Panel::IsFullscreen() const {
  return false;
}

bool Panel::IsFullscreenBubbleVisible() const {
  NOTIMPLEMENTED();
  return false;
}

LocationBar* Panel::GetLocationBar() const {
  // Panels do not have a location bar.
  return NULL;
}

void Panel::SetFocusToLocationBar(bool select_all) {
  // Panels do not have a location bar.
}

void Panel::UpdateReloadStopState(bool is_loading, bool force) {
  // TODO(jennb): Implement.
}

void Panel::UpdateToolbar(TabContentsWrapper* contents,
                          bool should_restore_state) {
  // Panels do not have a toolbar.
}

void Panel::FocusToolbar() {
  // Panels do not have a toolbar.
}

void Panel::FocusAppMenu() {
  NOTIMPLEMENTED();
}

void Panel::FocusBookmarksToolbar() {
  NOTIMPLEMENTED();
}

void Panel::FocusChromeOSStatus() {
  NOTIMPLEMENTED();
}

void Panel::RotatePaneFocus(bool forwards) {
  NOTIMPLEMENTED();
}

bool Panel::IsBookmarkBarVisible() const {
  return false;
}

bool Panel::IsBookmarkBarAnimating() const {
  return false;
}

// This is used by extensions to decide if a window can be closed.
// Always return true as panels do not have tabs that can be dragged,
// during which extensions will avoid closing a window.
bool Panel::IsTabStripEditable() const {
  return true;
}

bool Panel::IsToolbarVisible() const {
  NOTIMPLEMENTED();
  return false;
}

void Panel::DisableInactiveFrame() {
  NOTIMPLEMENTED();
}

void Panel::ConfirmSetDefaultSearchProvider(
    TabContents* tab_contents,
    TemplateURL* template_url,
    TemplateURLService* template_url_service) {
  NOTIMPLEMENTED();
}

void Panel::ConfirmAddSearchProvider(const TemplateURL* template_url,
                                     Profile* profile) {
  NOTIMPLEMENTED();
}

void Panel::ToggleBookmarkBar() {
  NOTIMPLEMENTED();
}

void Panel::ShowAboutChromeDialog() {
  NOTIMPLEMENTED();
}

void Panel::ShowUpdateChromeDialog() {
  NOTIMPLEMENTED();
}

void Panel::ShowTaskManager() {
  native_panel_->ShowTaskManagerForPanel();
}

void Panel::ShowBackgroundPages() {
  NOTIMPLEMENTED();
}

void Panel::ShowBookmarkBubble(const GURL& url, bool already_bookmarked) {
  NOTIMPLEMENTED();
}

bool Panel::IsDownloadShelfVisible() const {
  return false;
}

DownloadShelf* Panel::GetDownloadShelf() {
  Browser* panel_browser = native_panel_->GetPanelBrowser();
  Profile* profile = panel_browser->GetProfile();
  Browser* tabbed_browser = Browser::GetTabbedBrowser(profile, true);

  if (!tabbed_browser) {
    // Set initial bounds so window will not be positioned at an offset
    // to this panel as panels are at the bottom of the screen.
    gfx::Rect window_bounds;
    bool maximized;
    WindowSizer::GetBrowserWindowBounds(
        std::string(), gfx::Rect(), panel_browser, &window_bounds, &maximized);
    Browser::CreateParams params(Browser::TYPE_TABBED, profile);
    params.initial_bounds = window_bounds;
    tabbed_browser = Browser::CreateWithParams(params);
    tabbed_browser->NewTab();
  }

  tabbed_browser->window()->Show();  // Ensure download shelf is visible.
  return tabbed_browser->window()->GetDownloadShelf();
}

void Panel::ShowRepostFormWarningDialog(TabContents* tab_contents) {
  NOTIMPLEMENTED();
}

void Panel::ShowCollectedCookiesDialog(TabContentsWrapper* wrapper) {
  NOTIMPLEMENTED();
}

void Panel::ShowThemeInstallBubble() {
  NOTIMPLEMENTED();
}

void Panel::ConfirmBrowserCloseWithPendingDownloads() {
  NOTIMPLEMENTED();
}

gfx::NativeWindow Panel::ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
                                        gfx::NativeWindow parent_window) {
  NOTIMPLEMENTED();
  return NULL;
}

void Panel::UserChangedTheme() {
  native_panel_->NotifyPanelOnUserChangedTheme();
}

int Panel::GetExtraRenderViewHeight() const {
  NOTIMPLEMENTED();
  return -1;
}

void Panel::TabContentsFocused(TabContents* tab_contents) {
  NOTIMPLEMENTED();
}

void Panel::ShowPageInfo(Profile* profile,
                         const GURL& url,
                         const NavigationEntry::SSLStatus& ssl,
                         bool show_history) {
  NOTIMPLEMENTED();
}

void Panel::ShowAppMenu() {
  NOTIMPLEMENTED();
}

bool Panel::PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                   bool* is_keyboard_shortcut) {
  return native_panel_->PreHandlePanelKeyboardEvent(event,
                                                    is_keyboard_shortcut);
}

void Panel::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  native_panel_->HandlePanelKeyboardEvent(event);
}

void Panel::ShowCreateWebAppShortcutsDialog(TabContentsWrapper* tab_contents) {
  NOTIMPLEMENTED();
}

void Panel::ShowCreateChromeAppShortcutsDialog(Profile* profile,
                                               const Extension* app) {
  NOTIMPLEMENTED();
}

void Panel::ToggleUseCompactNavigationBar() {
  NOTIMPLEMENTED();
}

void Panel::Cut() {
  NOTIMPLEMENTED();
}

void Panel::Copy() {
  NOTIMPLEMENTED();
}

void Panel::Paste() {
  NOTIMPLEMENTED();
}

void Panel::ToggleTabStripMode() {
  NOTIMPLEMENTED();
}

#if defined(OS_MACOSX)
void Panel::OpenTabpose() {
  NOTIMPLEMENTED();
}

void Panel::SetPresentationMode(bool presentation_mode) {
  NOTIMPLEMENTED();
}

bool Panel::InPresentationMode() {
  NOTIMPLEMENTED();
  return false;
}
#endif

void Panel::PrepareForInstant() {
  NOTIMPLEMENTED();
}

void Panel::ShowInstant(TabContentsWrapper* preview) {
  NOTIMPLEMENTED();
}

void Panel::HideInstant(bool instant_is_active) {
  NOTIMPLEMENTED();
}

gfx::Rect Panel::GetInstantBounds() {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

WindowOpenDisposition Panel::GetDispositionForPopupBounds(
    const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
  return NEW_POPUP;
}

FindBar* Panel::CreateFindBar() {
  return native_panel_->CreatePanelFindBar();
}

#if defined(OS_CHROMEOS)
void Panel::ShowKeyboardOverlay(gfx::NativeWindow owning_window) {
  NOTIMPLEMENTED();
}
#endif

void Panel::UpdatePreferredSize(TabContents* tab_contents,
                                const gfx::Size& pref_size) {
  gfx::Size non_client_size = native_panel_->GetNonClientAreaExtent();
  return manager()->OnPreferredWindowSizeChanged(this,
      gfx::Size(pref_size.width() + non_client_size.width(),
                pref_size.height() + non_client_size.height()));
}

void Panel::Observe(int type,
                    const NotificationSource& source,
                    const NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_TAB_ADDED:
    case content::NOTIFICATION_TAB_CONTENTS_SWAPPED: {
      RenderViewHost* render_view_host = GetRenderViewHost();
      DCHECK(render_view_host);
      render_view_host->Send(new ViewMsg_EnablePreferredSizeChangedMode(
          render_view_host->routing_id(),
          kPreferredSizeWidth | kPreferredSizeHeightThisIsSlow));
      RequestRenderViewHostToDisableScrollbars(render_view_host);
      break;
    }
    default:
      NOTREACHED() << "Got a notification we didn't register for!";
      break;
  }
}

RenderViewHost* Panel::GetRenderViewHost() const {
  TabContents* tab_contents = browser()->GetSelectedTabContents();
  if (!tab_contents)
    return NULL;
  return tab_contents->render_view_host();
}

void Panel::RequestRenderViewHostToDisableScrollbars(
    RenderViewHost* render_view_host) {
  DCHECK(render_view_host);

  gfx::Size non_client_size = native_panel_->GetNonClientAreaExtent();
  render_view_host->Send(new ViewMsg_DisableScrollbarsForSmallWindows(
      render_view_host->routing_id(),
      gfx::Size(max_size_.width() - non_client_size.width(),
                max_size_.height() - non_client_size.height())));
}

Browser* Panel::browser() const {
  return native_panel_->GetPanelBrowser();
}

void Panel::DestroyBrowser() {
  native_panel_->DestroyPanelBrowser();
}
