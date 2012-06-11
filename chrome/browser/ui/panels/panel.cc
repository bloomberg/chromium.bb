// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_strip.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/rect.h"

using content::RenderViewHost;

Panel::Panel(const std::string& app_name,
             const gfx::Size& min_size, const gfx::Size& max_size)
    : app_name_(app_name),
      panel_strip_(NULL),
      initialized_(false),
      min_size_(min_size),
      max_size_(max_size),
      max_size_policy_(DEFAULT_MAX_SIZE),
      auto_resizable_(false),
      always_on_top_(false),
      in_preview_mode_(false),
      native_panel_(NULL),
      attention_mode_(USE_PANEL_ATTENTION),
      expansion_state_(EXPANDED),
      command_updater_(this) {
}

Panel::~Panel() {
  // Invoked by native panel destructor. Do not access native_panel_ here.
}

void Panel::Initialize(const gfx::Rect& bounds, Browser* browser) {
  DCHECK(!initialized_);
  DCHECK(!panel_strip_);  // Cannot be added to a strip until fully created.
  DCHECK_EQ(EXPANDED, expansion_state_);
  DCHECK(!bounds.IsEmpty());
  initialized_ = true;
  full_size_ = bounds.size();
  native_panel_ = CreateNativePanel(browser, this, bounds);
}

void Panel::OnNativePanelClosed() {
  manager()->OnPanelClosed(this);
  DCHECK(!panel_strip_);
}

PanelManager* Panel::manager() const {
  return PanelManager::GetInstance();
}

Browser* Panel::browser() const {
  return NULL;
}

BrowserWindow* Panel::browser_window() const {
  return NULL;
}

CommandUpdater* Panel::command_updater() {
  return &command_updater_;
}

Profile* Panel::profile() const {
  // TODO(jennb): implement.
  return NULL;
}

content::WebContents* Panel::WebContents() const {
  // TODO(jennb): implement.
  return NULL;
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
}

void Panel::SetPanelBoundsInstantly(const gfx::Rect& bounds) {
  native_panel_->SetPanelBoundsInstantly(bounds);
}

void Panel::LimitSizeToDisplayArea(const gfx::Rect& display_area) {
  int max_width = manager()->GetMaxPanelWidth();
  int max_height = manager()->GetMaxPanelHeight();

  // If the custom max size is used, ensure that it does not exceed the display
  // area.
  if (max_size_policy_ == CUSTOM_MAX_SIZE) {
    int current_max_width = max_size_.width();
    if (current_max_width > max_width)
      max_width = std::min(current_max_width, display_area.width());
    int current_max_height = max_size_.height();
    if (current_max_height > max_height)
      max_height = std::min(current_max_height, display_area.height());
  }

  SetSizeRange(min_size_, gfx::Size(max_width, max_height));

  // Ensure that full size does not exceed max size.
  full_size_ = ClampSize(full_size_);
}

void Panel::SetAutoResizable(bool resizable) {
  if (auto_resizable_ == resizable)
    return;

  auto_resizable_ = resizable;
  content::WebContents* web_contents = WebContents();
  if (auto_resizable_) {
    if (web_contents)
      EnableWebContentsAutoResize(web_contents);
  } else {
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

  ConfigureAutoResize(WebContents());
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
  native_panel_->PanelExpansionStateChanging(expansion_state_, new_state);
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
  if (!panel_strip_)
    return;
  panel_strip_->ResizePanelWindow(this, bounds.size());
  SetAutoResizable(false);
}

// Close() may be called multiple times if the panel window is not ready to
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

void Panel::SetDraggableRegion(SkRegion* region) {
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

gfx::NativeWindow Panel::GetNativeWindow() {
  return native_panel_->GetNativePanelHandle();
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

bool Panel::IsFullscreen() const {
  return false;
}

void Panel::OnWindowAutoResized(const gfx::Size& preferred_window_size) {
  DCHECK(auto_resizable_);
  if (panel_strip_)
    panel_strip_->ResizePanelWindow(this, preferred_window_size);
}

void Panel::OnWindowResizedByMouse(const gfx::Rect& new_bounds) {
  if (panel_strip_)
    panel_strip_->OnPanelResizedByMouse(this, new_bounds);
}

void Panel::EnableWebContentsAutoResize(content::WebContents* web_contents) {
  DCHECK(web_contents);
  ConfigureAutoResize(web_contents);

  // We also need to know when the render view host changes in order
  // to turn on auto-resize notifications in the new render view host.
  registrar_.RemoveAll();  // Stop notifications for previous contents, if any.
  registrar_.Add(
      this,
      content::NOTIFICATION_WEB_CONTENTS_SWAPPED,
      content::Source<content::WebContents>(web_contents));
}

void Panel::ExecuteCommandWithDisposition(int id,
                                          WindowOpenDisposition disposition) {
  // TODO(jennb): implement.
}

bool Panel::ExecuteCommandIfEnabled(int id) {
  if (command_updater()->SupportsCommand(id) &&
      command_updater()->IsCommandEnabled(id)) {
    ExecuteCommandWithDisposition(id, CURRENT_TAB);
    return true;
  }
  return false;
}

void Panel::Observe(int type,
                    const content::NotificationSource& source,
                    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_SWAPPED, type);
  ConfigureAutoResize(content::Source<content::WebContents>(source).ptr());
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

void Panel::ConfigureAutoResize(content::WebContents* web_contents) {
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
  ConfigureAutoResize(WebContents());
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
  // Clicking the restore button has the same behavior as clicking the titlebar.
  OnTitlebarClicked(modifier);
}

void Panel::OnPanelStartUserResizing() {
  SetAutoResizable(false);
  SetPreviewMode(true);
  max_size_policy_ = CUSTOM_MAX_SIZE;
}

void Panel::OnPanelEndUserResizing() {
  SetPreviewMode(false);
}

bool Panel::ShouldCloseWindow() {
  // TODO(jennb): implement
  return true;
}

void Panel::OnWindowClosing() {
  // TODO(jennb): implement
}

string16 Panel::GetWindowTitle() const {
  content::WebContents* contents = WebContents();
  string16 title;

  // |contents| can be NULL during the window's creation.
  if (contents) {
    title = contents->GetTitle();
    FormatTitleForDisplay(&title);
  }

  if (title.empty())
    title = UTF8ToUTF16(app_name());

  return title;
}

// static
void Panel::FormatTitleForDisplay(string16* title) {
  size_t current_index = 0;
  size_t match_index;
  while ((match_index = title->find(L'\n', current_index)) != string16::npos) {
    title->replace(match_index, 1, string16());
    current_index = match_index;
  }
}

SkBitmap Panel::GetCurrentPageIcon() const {
  // TODO(jennb): implement.
  return SkBitmap();
}
