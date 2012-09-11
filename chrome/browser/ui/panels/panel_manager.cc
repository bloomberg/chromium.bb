// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
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

#if defined(OS_WIN)
#include "base/win/metro.h"
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

// Width to height ratio is used to compute the default width or height
// when only one value is provided.
const double kPanelDefaultWidthToHeightRatio = 1.62;  // golden ratio
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

#if defined(OS_WIN)
  // No panels in Metro mode.
  if (base::win::IsMetroProcess())
    return false;
#endif // OS_WIN

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

gfx::Point PanelManager::GetDefaultDetachedPanelOrigin() {
  return detached_strip_->GetDefaultPanelOrigin();
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

Panel* PanelManager::CreatePanel(const std::string& app_name,
                                 Profile* profile,
                                 const GURL& url,
                                 const gfx::Rect& requested_bounds,
                                 CreateMode mode) {
  // Need to sync the display area if no panel is present. This is because:
  // 1) Display area is not initialized until first panel is created.
  // 2) On windows, display settings notification is tied to a window. When
  //    display settings are changed at the time that no panel exists, we do
  //    not receive any notification.
  if (num_panels() == 0) {
    display_settings_provider_->OnDisplaySettingsChanged();
    display_settings_provider_->AddFullScreenObserver(this);
  }

  // Compute initial bounds for the panel.
  int width = requested_bounds.width();
  int height = requested_bounds.height();
  if (width == 0)
    width = height * kPanelDefaultWidthToHeightRatio;
  else if (height == 0)
    height = width / kPanelDefaultWidthToHeightRatio;

  gfx::Size min_size(panel::kPanelMinWidth, panel::kPanelMinHeight);
  gfx::Size max_size(GetMaxPanelWidth(), GetMaxPanelHeight());
  if (width < min_size.width())
    width = min_size.width();
  else if (width > max_size.width())
    width = max_size.width();

  if (height < min_size.height())
    height = min_size.height();
  else if (height > max_size.height())
    height = max_size.height();

  gfx::Rect bounds(width, height);
  if (CREATE_AS_DOCKED == mode) {
    bounds.set_origin(docked_strip_->GetDefaultPositionForPanel(bounds.size()));
  } else {
    bounds.set_origin(requested_bounds.origin());
    bounds = bounds.AdjustToFit(display_settings_provider_->GetDisplayArea());
  }

  // Create the panel.
  Panel* panel = new Panel(app_name, min_size, max_size);
  panel->Initialize(profile, url, bounds);

  // Auto resizable feature is enabled only if no initial size is requested.
  if (auto_sizing_enabled() && requested_bounds.width() == 0 &&
      requested_bounds.height() == 0) {
    panel->SetAutoResizable(true);
  }

  // Add the panel to the appropriate panel strip.
  // Delay layout refreshes in case multiple panels are created within
  // a short time of one another or the focus changes shortly after panel
  // is created to avoid excessive screen redraws.
  PanelStrip* panel_strip;
  PanelStrip::PositioningMask positioning_mask;
  if (CREATE_AS_DOCKED == mode) {
    panel_strip = docked_strip_.get();
    positioning_mask = PanelStrip::DELAY_LAYOUT_REFRESH;
  } else {
    panel_strip = detached_strip_.get();
    positioning_mask = PanelStrip::KNOWN_POSITION;
  }

  panel_strip->AddPanel(panel, positioning_mask);
  panel_strip->UpdatePanelOnStripChange(panel);

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
