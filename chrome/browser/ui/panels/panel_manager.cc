// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/panels/detached_panel_strip.h"
#include "chrome/browser/ui/panels/docked_panel_strip.h"
#include "chrome/browser/ui/panels/panel_drag_controller.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"
#include "chrome/browser/ui/panels/panel_resize_controller.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

#if defined(TOOLKIT_GTK)
#include "ui/base/x/x11_util.h"
#endif

namespace {
// Width of spacing around panel strip and the left/right edges of the screen.
const int kPanelStripLeftMargin = 6;
const int kPanelStripRightMargin = 24;

// Maxmium width of a panel is based on a factor of the working area.
#if defined(OS_CHROMEOS)
// ChromeOS device screens are relatively small and limiting the width
// interferes with some apps (e.g. http://crbug.com/111121).
const double kPanelMaxWidthFactor = 0.80;
#else
const double kPanelMaxWidthFactor = 0.35;
#endif

// Maxmium height of a panel is based on a factor of the working area.
const double kPanelMaxHeightFactor = 0.5;
}  // namespace

// static
bool PanelManager::shorten_time_intervals_ = false;

// static
PanelManager* PanelManager::GetInstance() {
  static base::LazyInstance<PanelManager> instance = LAZY_INSTANCE_INITIALIZER;
  return instance.Pointer();
}

// static
bool PanelManager::ShouldUsePanels(const std::string& extension_id) {
#if defined(TOOLKIT_GTK)
  // Panels are only supported on a white list of window managers for Linux.
  ui::WindowManagerName wm_type = ui::GuessWindowManager();
  if (wm_type != ui::WM_COMPIZ &&
      wm_type != ui::WM_ICE_WM &&
      wm_type != ui::WM_KWIN &&
      wm_type != ui::WM_METACITY &&
      wm_type != ui::WM_MUTTER &&
      wm_type != ui::WM_XFWM4) {
    return false;
  }
#endif  // TOOLKIT_GTK

  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE ||
      channel == chrome::VersionInfo::CHANNEL_BETA) {
    return CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnablePanels) ||
        extension_id == std::string("nckgahadagoaajjgafhacjanaoiihapd") ||
        extension_id == std::string("ljclpkphhpbpinifbeabbhlfddcpfdde") ||
        extension_id == std::string("ppleadejekpmccmnpjdimmlfljlkdfej") ||
        extension_id == std::string("eggnbpckecmjlblplehfpjjdhhidfdoj");
  }

  return true;
}

PanelManager::PanelManager()
    : panel_mouse_watcher_(PanelMouseWatcher::Create()),
      auto_sizing_enabled_(true) {
  // DisplaySettingsProvider should be created before the creation of strips
  // since some strip might depend on it.
  display_settings_provider_.reset(DisplaySettingsProvider::Create());
  display_settings_provider_->AddDisplayAreaObserver(this);

  detached_strip_.reset(new DetachedPanelStrip(this));
  docked_strip_.reset(new DockedPanelStrip(this));
  drag_controller_.reset(new PanelDragController(this));
  resize_controller_.reset(new PanelResizeController(this));
}

PanelManager::~PanelManager() {
  display_settings_provider_->RemoveDisplayAreaObserver(this);

  // Docked strip should be disposed explicitly before DisplaySettingsProvider
  // is gone since docked strip needs to remove the observer from
  // DisplaySettingsProvider.
  docked_strip_.reset();
}

void PanelManager::OnDisplayAreaChanged(const gfx::Rect& display_area) {
  if (display_area == display_area_)
    return;
  display_area_ = display_area;

  gfx::Rect docked_strip_bounds = display_area;
  docked_strip_bounds.set_x(display_area.x() + kPanelStripLeftMargin);
  docked_strip_bounds.set_width(display_area.width() -
                                kPanelStripLeftMargin - kPanelStripRightMargin);
  docked_strip_->SetDisplayArea(docked_strip_bounds);

  detached_strip_->SetDisplayArea(display_area);
}

void PanelManager::OnFullScreenModeChanged(bool is_full_screen) {
  docked_strip_->OnFullScreenModeChanged(is_full_screen);
}

int PanelManager::GetMaxPanelWidth() const {
  return static_cast<int>(display_area_.width() * kPanelMaxWidthFactor);
}

int PanelManager::GetMaxPanelHeight() const {
  return display_area_.height() * kPanelMaxHeightFactor;
}

Panel* PanelManager::CreatePanel(Browser* browser) {
  // Need to sync the display area if no panel is present. This is because:
  // 1) Display area is not initialized until first panel is created.
  // 2) On windows, display settings notification is tied to a window. When
  //    display settings are changed at the time that no panel exists, we do
  //    not receive any notification.
  if (num_panels() == 0)
    display_settings_provider_->OnDisplaySettingsChanged();

  int width = browser->override_bounds().width();
  int height = browser->override_bounds().height();
  Panel* panel = new Panel(browser, gfx::Size(width, height));
  docked_strip_->AddPanel(panel, PanelStrip::DEFAULT_POSITION);
  docked_strip_->UpdatePanelOnStripChange(panel);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_ADDED,
      content::Source<Panel>(panel),
      content::NotificationService::NoDetails());

  if (num_panels() == 1) {
    display_settings_provider_->AddFullScreenObserver(this);
  }

  return panel;
}

void PanelManager::OnPanelClosed(Panel* panel) {
  if (num_panels() == 1) {
    display_settings_provider_->RemoveFullScreenObserver(this);
  }

  drag_controller_->OnPanelClosed(panel);
  resize_controller_->OnPanelClosed(panel);
  panel->panel_strip()->RemovePanel(panel);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_CLOSED,
      content::Source<Panel>(panel),
      content::NotificationService::NoDetails());
}

void PanelManager::StartDragging(Panel* panel,
                                 const gfx::Point& mouse_location) {
  drag_controller_->StartDragging(panel, mouse_location);
}

void PanelManager::Drag(const gfx::Point& mouse_location) {
  drag_controller_->Drag(mouse_location);
}

void PanelManager::EndDragging(bool cancelled) {
  drag_controller_->EndDragging(cancelled);
}

void PanelManager::StartResizingByMouse(Panel* panel,
    const gfx::Point& mouse_location,
    panel::ResizingSides sides) {
  if (panel->CanResizeByMouse() != panel::NOT_RESIZABLE &&
      sides != panel::RESIZE_NONE)
    resize_controller_->StartResizing(panel, mouse_location, sides);
}

void PanelManager::ResizeByMouse(const gfx::Point& mouse_location) {
  if (resize_controller_->IsResizing())
    resize_controller_->Resize(mouse_location);
}

void PanelManager::EndResizingByMouse(bool cancelled) {
  if (resize_controller_->IsResizing()) {
    Panel* resized_panel = resize_controller_->EndResizing(cancelled);
    if (!cancelled && resized_panel->panel_strip())
      resized_panel->panel_strip()->RefreshLayout();
  }
}

void PanelManager::OnPanelExpansionStateChanged(Panel* panel) {
  // For panels outside of the docked strip changing state is a no-op.
  // But since this method may be called for panels in other strips
  // we need to check this condition.
  if (panel->panel_strip() == docked_strip_.get())
    docked_strip_->OnPanelExpansionStateChanged(panel);

}

void PanelManager::OnWindowAutoResized(Panel* panel,
                                       const gfx::Size& preferred_window_size) {
  DCHECK(auto_sizing_enabled_);
  panel->panel_strip()->ResizePanelWindow(panel, preferred_window_size);
}

void PanelManager::ResizePanel(Panel* panel, const gfx::Size& new_size) {
  PanelStrip* panel_strip = panel->panel_strip();
  if (!panel_strip)
    return;
  panel_strip->ResizePanelWindow(panel, new_size);
  panel->SetAutoResizable(false);
}

void PanelManager::OnPanelResizedByMouse(Panel* panel,
                                         const gfx::Rect& new_bounds) {
  panel->panel_strip()->OnPanelResizedByMouse(panel, new_bounds);
}

void PanelManager::MovePanelToStrip(
    Panel* panel,
    PanelStrip::Type new_layout,
    PanelStrip::PositioningMask positioning_mask) {
  DCHECK(panel);
  PanelStrip* current_strip = panel->panel_strip();
  DCHECK(current_strip);
  DCHECK_NE(current_strip->type(), new_layout);
  current_strip->RemovePanel(panel);

  PanelStrip* target_strip = NULL;
  switch (new_layout) {
    case PanelStrip::DETACHED:
      target_strip = detached_strip_.get();
      break;
    case PanelStrip::DOCKED:
      target_strip = docked_strip_.get();
      break;
    default:
      NOTREACHED();
  }

  target_strip->AddPanel(panel, positioning_mask);
  target_strip->UpdatePanelOnStripChange(panel);
}

bool PanelManager::ShouldBringUpTitlebars(int mouse_x, int mouse_y) const {
  return docked_strip_->ShouldBringUpTitlebars(mouse_x, mouse_y);
}

void PanelManager::BringUpOrDownTitlebars(bool bring_up) {
  docked_strip_->BringUpOrDownTitlebars(bring_up);
}

BrowserWindow* PanelManager::GetNextBrowserWindowToActivate(
    Panel* panel) const {
  // Find the last active browser window that is not minimized.
  BrowserList::const_reverse_iterator iter = BrowserList::begin_last_active();
  BrowserList::const_reverse_iterator end = BrowserList::end_last_active();
  for (; (iter != end); ++iter) {
    Browser* browser = *iter;
    if (panel->browser() != browser && !browser->window()->IsMinimized())
      return browser->window();
  }

  return NULL;
}

void PanelManager::CloseAll() {
  DCHECK(!drag_controller_->IsDragging());

  detached_strip_->CloseAll();
  docked_strip_->CloseAll();
}

int PanelManager::num_panels() const {
  return detached_strip_->num_panels() + docked_strip_->num_panels();
}

std::vector<Panel*> PanelManager::panels() const {
  std::vector<Panel*> panels;
  for (DetachedPanelStrip::Panels::const_iterator iter =
           detached_strip_->panels().begin();
       iter != detached_strip_->panels().end(); ++iter)
    panels.push_back(*iter);
  for (DockedPanelStrip::Panels::const_iterator iter =
           docked_strip_->panels().begin();
       iter != docked_strip_->panels().end(); ++iter)
    panels.push_back(*iter);
  return panels;
}

void PanelManager::SetMouseWatcher(PanelMouseWatcher* watcher) {
  panel_mouse_watcher_.reset(watcher);
}

void PanelManager::OnPanelAnimationEnded(Panel* panel) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_BOUNDS_ANIMATIONS_FINISHED,
      content::Source<Panel>(panel),
      content::NotificationService::NoDetails());
}
