// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel.h"

#include "base/logging.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/window_sizer.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
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
      expansion_state_(EXPANDED),
      restored_height_(bounds.height()) {
  native_panel_ = CreateNativePanel(browser, this, bounds);
  browser->tabstrip_model()->AddObserver(this);
}

Panel::~Panel() {
  // Invoked by native panel destructor. Do not access native_panel_ here.
}

void Panel::OnNativePanelClosed() {
  native_panel_->GetPanelBrowser()->tabstrip_model()->RemoveObserver(this);
  manager()->Remove(this);
}

PanelManager* Panel::manager() const {
  return PanelManager::GetInstance();
}

const Extension* Panel::GetExtension() const {
  return GetExtensionFromBrowser(browser());
}

void Panel::SetPanelBounds(const gfx::Rect& bounds) {
  if (expansion_state_ == Panel::EXPANDED)
    restored_height_ = bounds.height();

  native_panel_->SetPanelBounds(bounds);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_CHANGED_BOUNDS,
      content::Source<Panel>(this),
      content::NotificationService::NoDetails());
}

void Panel::SetMaxSize(const gfx::Size& max_size) {
  if (max_size_ == max_size)
    return;
  max_size_ = max_size;

  // Note: |render_view_host| might be NULL if the tab has not been created.
  // If so, we will do it when TabInsertedAt() is invoked.
  RenderViewHost* render_view_host = GetRenderViewHost();
  if (render_view_host)
    RequestRenderViewHostToDisableScrollbars(render_view_host);
}

void Panel::SetExpansionState(ExpansionState new_state) {
  if (expansion_state_ == new_state)
    return;

  ExpansionState old_state = expansion_state_;
  expansion_state_ = new_state;

  int height;
  switch (expansion_state_) {
    case EXPANDED:
      height = restored_height_;
      break;
    case TITLE_ONLY:
      height = native_panel_->TitleOnlyHeight();
      break;
    case MINIMIZED:
      height = kMinimizedPanelHeight;
      break;
    default:
      NOTREACHED();
      height = restored_height_;
      break;
  }

  int bottom = manager()->GetBottomPositionForExpansionState(expansion_state_);
  gfx::Rect bounds = native_panel_->GetPanelBounds();
  bounds.set_y(bottom - height);
  bounds.set_height(height);
  SetPanelBounds(bounds);

  manager()->OnPanelExpansionStateChanged(old_state, new_state);

  // The minimized panel should not get the focus.
  if (expansion_state_ == MINIMIZED)
    Deactivate();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_CHANGED_EXPANSION_STATE,
      content::Source<Panel>(this),
      content::NotificationService::NoDetails());
}

bool Panel::ShouldBringUpTitlebar(int mouse_x, int mouse_y) const {
  // Skip the expanded panel.
  if (expansion_state_ == EXPANDED)
    return false;

  // If the panel is showing titlebar only, we want to keep it up when it is
  // being dragged.
  if (expansion_state_ == TITLE_ONLY && manager()->is_dragging_panel())
    return true;

  // We do not want to bring up other minimized panels if the mouse is over the
  // panel that pops up the title-bar to attract attention.
  if (native_panel_->IsDrawingAttention())
    return false;

  gfx::Rect bounds = native_panel_->GetPanelBounds();
  return bounds.x() <= mouse_x && mouse_x <= bounds.right() &&
         mouse_y >= bounds.y();
}

bool Panel::IsDrawingAttention() const {
  return native_panel_->IsDrawingAttention();
}

int Panel::GetRestoredHeight() const {
  return restored_height_;
}

void Panel::SetRestoredHeight(int height) {
  restored_height_ = height;
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
}

void Panel::Activate() {
  // Make sure the panel is expanded when activated programmatically,
  // so the user input does not go into collapsed window.
  SetExpansionState(Panel::EXPANDED);
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
  // TODO(jennb): Implement. http://crbug.com/102723
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
  native_panel_->UpdatePanelLoadingAnimations(should_animate);
}

void Panel::SetStarredState(bool is_starred) {
  // Since panels are typically not bookmarked extension UI windows, they don't
  // have starred state.
}

gfx::Rect Panel::GetRestoredBounds() const {
  gfx::Rect bounds = native_panel_->GetPanelBounds();
  bounds.set_y(bounds.y() + bounds.height() - restored_height_);
  bounds.set_height(restored_height_);
  return bounds;
}

gfx::Rect Panel::GetBounds() const {
  return native_panel_->GetPanelBounds();
}

bool Panel::IsMaximized() const {
  // Size of panels is managed by PanelManager, they are never 'zoomed'.
  return false;
}

bool Panel::IsMinimized() const {
  return expansion_state_ != EXPANDED;
}

void Panel::EnterFullscreen(
      const GURL& url, FullscreenExitBubbleType type) {
  NOTIMPLEMENTED();
}

void Panel::ExitFullscreen() {
  NOTIMPLEMENTED();
}

void Panel::UpdateFullscreenExitBubbleContent(
      const GURL& url,
      FullscreenExitBubbleType bubble_type) {
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
  // Panels don't have stop/reload indicator.
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

void Panel::ConfirmSetDefaultSearchProvider(TabContents* tab_contents,
                                            TemplateURL* template_url,
                                            Profile* profile) {
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
    WindowSizer::GetBrowserWindowBounds(std::string(), gfx::Rect(),
                                        panel_browser, &window_bounds);
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

void Panel::ConfirmBrowserCloseWithPendingDownloads() {
  NOTIMPLEMENTED();
}

void Panel::UserChangedTheme() {
  native_panel_->NotifyPanelOnUserChangedTheme();
}

int Panel::GetExtraRenderViewHeight() const {
  // This is currently used only on Linux and that returns the height for
  // optional elements like bookmark bar, download bar etc.  Not applicable to
  // panels.
  return 0;
}

void Panel::TabContentsFocused(TabContents* tab_contents) {
  native_panel_->PanelTabContentsFocused(tab_contents);
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

void Panel::Cut() {
  native_panel_->PanelCut();
}

void Panel::Copy() {
  native_panel_->PanelCopy();
}

void Panel::Paste() {
  native_panel_->PanelPaste();
}

#if defined(OS_MACOSX)
void Panel::OpenTabpose() {
  NOTIMPLEMENTED();
}

void Panel::EnterPresentationMode(
      const GURL& url,
      FullscreenExitBubbleType content_type) {
  NOTIMPLEMENTED();
}

void Panel::ExitPresentationMode() {
  NOTIMPLEMENTED();
}

bool Panel::InPresentationMode() {
  NOTIMPLEMENTED();
  return false;
}
#endif

void Panel::ShowInstant(TabContentsWrapper* preview) {
  NOTIMPLEMENTED();
}

void Panel::HideInstant() {
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
void Panel::ShowMobileSetup() {
  NOTIMPLEMENTED();
}

void Panel::ShowKeyboardOverlay(gfx::NativeWindow owning_window) {
  NOTIMPLEMENTED();
}
#endif

void Panel::UpdatePreferredSize(TabContents* tab_contents,
                                const gfx::Size& pref_size) {
  return manager()->OnPreferredWindowSizeChanged(this,
      native_panel_->WindowSizeFromContentSize(pref_size));
}

void Panel::ShowAvatarBubble(TabContents* tab_contents, const gfx::Rect& rect) {
  // Panels will never show a new tab page so this should never be called.
  NOTREACHED();
}

void Panel::ShowAvatarBubbleFromAvatarButton() {
  // Panels will never show an avatar button so this should never be called.
  NOTREACHED();
}

void Panel::TabInsertedAt(TabContentsWrapper* contents,
                          int index,
                          bool foreground) {
  DCHECK_EQ(0, index);
  TabContents* tab_contents = contents->tab_contents();
  EnableAutoResize(tab_contents->render_view_host());

  // We also need to know when the render view host changes in order
  // to turn on preferred size changed notifications in the new
  // render view host.
  registrar_.RemoveAll();  // Stop notifications for previous contents, if any.
  registrar_.Add(
      this,
      content::NOTIFICATION_TAB_CONTENTS_SWAPPED,
      content::Source<TabContents>(tab_contents));
}

void Panel::Observe(int type,
                    const content::NotificationSource& source,
                    const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_TAB_CONTENTS_SWAPPED);
  RenderViewHost* render_view_host =
      content::Source<TabContents>(source).ptr()->render_view_host();
  if (render_view_host)
    EnableAutoResize(render_view_host);
}

RenderViewHost* Panel::GetRenderViewHost() const {
  TabContents* tab_contents = browser()->GetSelectedTabContents();
  if (!tab_contents)
    return NULL;
  return tab_contents->render_view_host();
}

void Panel::EnableAutoResize(RenderViewHost* render_view_host) {
  DCHECK(render_view_host);
  render_view_host->EnablePreferredSizeMode(
      kPreferredSizeWidth | kPreferredSizeHeightThisIsSlow);
  RequestRenderViewHostToDisableScrollbars(render_view_host);
}

void Panel::RequestRenderViewHostToDisableScrollbars(
    RenderViewHost* render_view_host) {
  DCHECK(render_view_host);
  render_view_host->DisableScrollbarsForThreshold(
      native_panel_->ContentSizeFromWindowSize(max_size_));
}

void Panel::OnWindowSizeAvailable() {
  RenderViewHost* render_view_host = GetRenderViewHost();
  if (render_view_host)
    RequestRenderViewHostToDisableScrollbars(render_view_host);
}

Browser* Panel::browser() const {
  return native_panel_->GetPanelBrowser();
}

void Panel::DestroyBrowser() {
  native_panel_->DestroyPanelBrowser();
}
