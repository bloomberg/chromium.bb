// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_strip.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/window_sizer.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/rect.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/panels/panel_browser_view.h"
#endif

using content::RenderViewHost;
using content::SSLStatus;
using content::WebContents;

Panel::Panel(Browser* browser, const gfx::Size& requested_size)
    : browser_(browser),
      panel_strip_(NULL),
      initialized_(false),
      full_size_(requested_size),
      auto_resizable_(false),
      always_on_top_(false),
      in_preview_mode_(false),
      native_panel_(NULL),
      attention_mode_(USE_PANEL_ATTENTION),
      expansion_state_(EXPANDED) {
}

Panel::~Panel() {
  // Invoked by native panel destructor. Do not access native_panel_ here.
}

void Panel::Initialize(const gfx::Rect& bounds) {
  DCHECK(!initialized_);
  DCHECK(!bounds.IsEmpty());
  initialized_ = true;
  native_panel_ = CreateNativePanel(browser_, this, bounds);
  if (panel_strip_ != NULL)
    native_panel_->PreventActivationByOS(panel_strip_->IsPanelMinimized(this));
}

void Panel::OnNativePanelClosed() {
  if (auto_resizable_)
    native_panel_->GetPanelBrowser()->tabstrip_model()->RemoveObserver(this);
  manager()->OnPanelClosed(this);
  DCHECK(!panel_strip_);
}

PanelManager* Panel::manager() const {
  return PanelManager::GetInstance();
}

bool Panel::CanMinimize() const {
  return panel_strip_ && panel_strip_->CanMinimizePanel(this) && !IsMinimized();
}

bool Panel::CanRestore() const {
  return panel_strip_ && panel_strip_->CanMinimizePanel(this) && IsMinimized();
}

panel::Resizability Panel::CanResizeByMouse() const {
  if (!panel_strip_)
    return panel::NOT_RESIZABLE;

  return panel_strip_->GetPanelResizability(this);
}

void Panel::SetPanelBounds(const gfx::Rect& bounds) {
  native_panel_->SetPanelBounds(bounds);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_CHANGED_BOUNDS,
      content::Source<Panel>(this),
      content::NotificationService::NoDetails());
}

void Panel::SetPanelBoundsInstantly(const gfx::Rect& bounds) {
  native_panel_->SetPanelBoundsInstantly(bounds);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_CHANGED_BOUNDS,
      content::Source<Panel>(this),
      content::NotificationService::NoDetails());
}

void Panel::SetAutoResizable(bool resizable) {
  if (auto_resizable_ == resizable)
    return;

  auto_resizable_ = resizable;
  WebContents* web_contents = browser()->GetSelectedWebContents();
  if (auto_resizable_) {
    browser()->tabstrip_model()->AddObserver(this);
    if (web_contents)
      EnableWebContentsAutoResize(web_contents);
  } else {
    browser()->tabstrip_model()->RemoveObserver(this);
    registrar_.RemoveAll();

    if (web_contents) {
      // NULL might be returned if the tab has not been added.
      RenderViewHost* render_view_host = web_contents->GetRenderViewHost();
      if (render_view_host)
        render_view_host->DisableAutoResize(full_size_);
    }
  }
}

void Panel::SetSizeRange(const gfx::Size& min_size, const gfx::Size& max_size) {
  if (min_size == min_size_ && max_size == max_size_)
    return;

  DCHECK(min_size.width() <= max_size.width());
  DCHECK(min_size.height() <= max_size.height());
  min_size_ = min_size;
  max_size_ = max_size;

  ConfigureAutoResize(browser()->GetSelectedWebContents());
}

void Panel::IncreaseMaxSize(const gfx::Size& desired_panel_size) {
  gfx::Size new_max_size = max_size_;
  if (new_max_size.width() < desired_panel_size.width())
    new_max_size.set_width(desired_panel_size.width());
  if (new_max_size.height() < desired_panel_size.height())
    new_max_size.set_height(desired_panel_size.height());

  SetSizeRange(min_size_, new_max_size);
}

gfx::Size Panel::ClampSize(const gfx::Size& size) const {

  // The panel width:
  // * cannot grow or shrink to go beyond [min_width, max_width]
  int new_width = size.width();
  if (new_width > max_size_.width())
    new_width = max_size_.width();
  if (new_width < min_size_.width())
    new_width = min_size_.width();

  // The panel height:
  // * cannot grow or shrink to go beyond [min_height, max_height]
  int new_height = size.height();
  if (new_height > max_size_.height())
    new_height = max_size_.height();
  if (new_height < min_size_.height())
    new_height = min_size_.height();

  return gfx::Size(new_width, new_height);
}

void Panel::SetAlwaysOnTop(bool on_top) {
  if (always_on_top_ == on_top)
    return;
  always_on_top_ = on_top;
  native_panel_->SetPanelAlwaysOnTop(on_top);
}

void Panel::EnableResizeByMouse(bool enable) {
  DCHECK(native_panel_);
  native_panel_->EnableResizeByMouse(enable);
}

void Panel::UpdateMinimizeRestoreButtonVisibility() {
  native_panel_->UpdatePanelMinimizeRestoreButtonVisibility();
}

void Panel::SetPreviewMode(bool in_preview) {
  DCHECK_NE(in_preview_mode_, in_preview);
  in_preview_mode_ = in_preview;
}

void Panel::SetExpansionState(ExpansionState new_state) {
  if (expansion_state_ == new_state)
    return;
  expansion_state_ = new_state;

  manager()->OnPanelExpansionStateChanged(this);

  DCHECK(initialized_ && panel_strip_ != NULL);
  native_panel_->PreventActivationByOS(panel_strip_->IsPanelMinimized(this));
  UpdateMinimizeRestoreButtonVisibility();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_CHANGED_EXPANSION_STATE,
      content::Source<Panel>(this),
      content::NotificationService::NoDetails());
}

bool Panel::IsDrawingAttention() const {
  return native_panel_->IsDrawingAttention();
}

void Panel::FullScreenModeChanged(bool is_full_screen) {
  native_panel_->FullScreenModeChanged(is_full_screen);
}

void Panel::Show() {
  if (manager()->display_settings_provider()->is_full_screen() || !panel_strip_)
    return;

    native_panel_->ShowPanel();
}

void Panel::ShowInactive() {
  if (manager()->display_settings_provider()->is_full_screen() || !panel_strip_)
    return;

  native_panel_->ShowPanelInactive();
}

void Panel::SetBounds(const gfx::Rect& bounds) {
  // Ignore bounds position as the panel manager controls all positioning.
  manager()->ResizePanel(this, bounds.size());
}

// Close() may be called multiple times if the browser window is not ready to
// close on the first attempt.
void Panel::Close() {
  native_panel_->ClosePanel();
}

void Panel::Activate() {
  if (!panel_strip_)
    return;

  panel_strip_->ActivatePanel(this);
  native_panel_->ActivatePanel();
}

void Panel::Deactivate() {
  native_panel_->DeactivatePanel();
}

bool Panel::IsActive() const {
  return native_panel_->IsPanelActive();
}

void Panel::FlashFrame(bool draw_attention) {
  if (IsDrawingAttention() == draw_attention || !panel_strip_)
    return;

  // Don't draw attention for an active panel.
  if (draw_attention && IsActive())
    return;

  // Invoking native panel to draw attention must be done before informing the
  // panel strip because it needs to check internal state of the panel to
  // determine if the panel has been drawing attention.
  native_panel_->DrawAttention(draw_attention);
  panel_strip_->OnPanelAttentionStateChanged(this);
}

bool Panel::IsAlwaysOnTop() const {
  return always_on_top_;
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

void Panel::SetDevToolsDockSide(DevToolsDockSide side) {
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
  bounds.set_y(bounds.bottom() - full_size_.height());
  bounds.set_x(bounds.right() - full_size_.width());
  bounds.set_size(full_size_);
  return bounds;
}

gfx::Rect Panel::GetBounds() const {
  return native_panel_->GetPanelBounds();
}

int Panel::TitleOnlyHeight() const {
  return native_panel_->TitleOnlyHeight();
}

void Panel::EnsureFullyVisible() {
  native_panel_->EnsurePanelFullyVisible();
}

bool Panel::IsMaximized() const {
  // Size of panels is managed by PanelManager, they are never 'zoomed'.
  return false;
}

bool Panel::IsMinimized() const {
  return !panel_strip_ || panel_strip()->IsPanelMinimized(this);
}

void Panel::Maximize() {
  Restore();
}

void Panel::Minimize() {
  if (panel_strip_)
    panel_strip_->MinimizePanel(this);
}

void Panel::Restore() {
  if (panel_strip_)
    panel_strip_->RestorePanel(this);
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
#if defined(USE_AURA)
  // TODO(stevenjb): Remove this when Aura panels are implemented post R18.
  PanelBrowserView* panel_view = static_cast<PanelBrowserView*>(native_panel_);
  return panel_view->GetLocationBar();
#else
  // Panels do not have a location bar.
  return NULL;
#endif
}

void Panel::SetFocusToLocationBar(bool select_all) {
#if defined(USE_AURA)
  // TODO(stevenjb): Remove this when Aura panels are implemented post R18.
  PanelBrowserView* panel_view = static_cast<PanelBrowserView*>(native_panel_);
  panel_view->SetFocusToLocationBar(select_all);
#else
  // Panels do not have a location bar.
#endif
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

gfx::Rect Panel::GetRootWindowResizerRect() const {
  return gfx::Rect();
}

bool Panel::IsPanel() const {
  return true;
}

void Panel::DisableInactiveFrame() {
  NOTIMPLEMENTED();
}

void Panel::ConfirmAddSearchProvider(TemplateURL* template_url,
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

void Panel::ShowChromeToMobileBubble() {
  NOTIMPLEMENTED();
}

#if defined(ENABLE_ONE_CLICK_SIGNIN)
void Panel::ShowOneClickSigninBubble(
      const base::Closure& learn_more_callback,
      const base::Closure& advanced_callback) {
  NOTIMPLEMENTED();
}
#endif

bool Panel::IsDownloadShelfVisible() const {
  return false;
}

DownloadShelf* Panel::GetDownloadShelf() {
  Browser* panel_browser = native_panel_->GetPanelBrowser();
  Profile* profile = panel_browser->profile();
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

void Panel::WebContentsFocused(WebContents* contents) {
  native_panel_->PanelWebContentsFocused(contents);
}

void Panel::ShowPageInfo(Profile* profile,
                         const GURL& url,
                         const SSLStatus& ssl,
                         bool show_history) {
  NOTIMPLEMENTED();
}
void Panel::ShowWebsiteSettings(Profile* profile,
                                TabContentsWrapper* tab_contents_wrapper,
                                const GURL& url,
                                const content::SSLStatus& ssl,
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
#if defined(USE_AURA)
  // TODO(stevenjb): Remove this when Aura panels are implemented post R18.
  PanelBrowserView* panel_view = static_cast<PanelBrowserView*>(native_panel_);
  return panel_view->GetDispositionForPopupBounds(bounds);
#else
  return NEW_POPUP;
#endif
}

FindBar* Panel::CreateFindBar() {
  return native_panel_->CreatePanelFindBar();
}

#if defined(OS_CHROMEOS)
void Panel::ShowKeyboardOverlay(gfx::NativeWindow owning_window) {
  NOTIMPLEMENTED();
}
#endif

void Panel::ResizeDueToAutoResize(WebContents* web_contents,
                                  const gfx::Size& pref_size) {
  DCHECK(auto_resizable_);
  return manager()->OnWindowAutoResized(
      this,
      native_panel_->WindowSizeFromContentSize(pref_size));
}

void Panel::ShowAvatarBubble(WebContents* web_contents, const gfx::Rect& rect) {
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
  if (auto_resizable_) {
    DCHECK_EQ(0, index);
    EnableWebContentsAutoResize(contents->web_contents());
  }
}

void Panel::EnableWebContentsAutoResize(WebContents* web_contents) {
  DCHECK(web_contents);
  ConfigureAutoResize(web_contents);

  // We also need to know when the render view host changes in order
  // to turn on auto-resize notifications in the new render view host.
  registrar_.RemoveAll();  // Stop notifications for previous contents, if any.
  registrar_.Add(
      this,
      content::NOTIFICATION_WEB_CONTENTS_SWAPPED,
      content::Source<WebContents>(web_contents));
}

void Panel::Observe(int type,
                    const content::NotificationSource& source,
                    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_SWAPPED, type);
  ConfigureAutoResize(content::Source<WebContents>(source).ptr());
}

void Panel::OnActiveStateChanged(bool active) {
  // Clear attention state when an expanded panel becomes active.
  // On some systems (e.g. Win), mouse-down activates a panel regardless of
  // its expansion state. However, we don't want to clear draw attention if
  // contents are not visible. In that scenario, if the mouse-down results
  // in a mouse-click, draw attention will be cleared then.
  // See Panel::OnTitlebarClicked().
  if (active && IsDrawingAttention() && !IsMinimized())
    FlashFrame(false);

  if (panel_strip_)
    panel_strip_->OnPanelActiveStateChanged(this);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_CHANGED_ACTIVE_STATUS,
      content::Source<Panel>(this),
      content::NotificationService::NoDetails());
}

void Panel::ConfigureAutoResize(WebContents* web_contents) {
  if (!auto_resizable_ || !web_contents)
    return;

  // NULL might be returned if the tab has not been added.
  RenderViewHost* render_view_host = web_contents->GetRenderViewHost();
  if (!render_view_host)
    return;

  render_view_host->EnableAutoResize(
      min_size_,
      native_panel_->ContentSizeFromWindowSize(max_size_));
}

void Panel::OnWindowSizeAvailable() {
  ConfigureAutoResize(browser()->GetSelectedWebContents());
}

void Panel::OnTitlebarClicked(panel::ClickModifier modifier) {
  if (panel_strip_)
    panel_strip_->OnPanelTitlebarClicked(this, modifier);

  // Normally the system activates a window when the titlebar is clicked.
  // However, we prevent system activation of minimized panels, thus the
  // activation may not have occurred. Also, some OSes (Windows) will
  // activate a minimized panel on mouse-down regardless of our attempts to
  // prevent system activation. Attention state is not cleared in that case.
  // See Panel::OnActiveStateChanged().
  // Therefore, we ensure activation and clearing of attention state here.
  // These are no-ops if no changes are needed.
  Activate();
  FlashFrame(false);
}

void Panel::OnMinimizeButtonClicked(panel::ClickModifier modifier) {
  if (!panel_strip_)
    return;

  if (modifier == panel::APPLY_TO_ALL)
    panel_strip_->MinimizeAll();
  else
    Minimize();
}

void Panel::OnRestoreButtonClicked(panel::ClickModifier modifier) {
  if (!panel_strip_)
    return;

  if (modifier == panel::APPLY_TO_ALL)
    panel_strip_->RestoreAll();
  else
    Restore();
}

void Panel::DestroyBrowser() {
  native_panel_->DestroyPanelBrowser();
}
