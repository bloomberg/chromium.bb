// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/panels/detached_panel_collection.h"
#include "chrome/browser/ui/panels/docked_panel_collection.h"
#include "chrome/browser/ui/panels/panel_drag_controller.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"
#include "chrome/browser/ui/panels/panel_resize_controller.h"
#include "chrome/browser/ui/panels/stacked_panel_collection.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

#if defined(TOOLKIT_GTK)
#include "ui/base/x/x11_util.h"
#endif

#if defined(OS_WIN)
#include "win8/util/win8_util.h"
#endif

namespace {
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

// When the stacking mode is enabled, the detached panel will be positioned
// near the top of the working area such that the subsequent panel could be
// stacked to the bottom of the detached panel. This value is experimental
// and subjective.
const int kDetachedPanelStartingYPositionOnStackingEnabled = 20;

// The following comparers are used by std::list<>::sort to determine which
// stack or panel we want to seacrh first for adding new panel.
bool ComparePanelsByPosition(Panel* panel1, Panel* panel2) {
  gfx::Rect bounds1 = panel1->GetBounds();
  gfx::Rect bounds2 = panel2->GetBounds();

  // When there're ties, the right-most stack will appear first.
  if (bounds1.x() > bounds2.x())
    return true;
  if (bounds1.x() < bounds2.x())
    return false;

  // In the event of another draw, the top-most stack will appear first.
  return bounds1.y() < bounds2.y();
}

bool ComparerNumberOfPanelsInStack(StackedPanelCollection* stack1,
                                   StackedPanelCollection* stack2) {
  // The stack with more panels will appear first.
  int num_panels_in_stack1 = stack1->num_panels();
  int num_panels_in_stack2 = stack2->num_panels();
  if (num_panels_in_stack1 > num_panels_in_stack2)
    return true;
  if (num_panels_in_stack1 < num_panels_in_stack2)
    return false;

  DCHECK(num_panels_in_stack1);

  return ComparePanelsByPosition(stack1->top_panel(), stack2->top_panel());
}

bool CompareDetachedPanels(Panel* panel1, Panel* panel2) {
  return ComparePanelsByPosition(panel1, panel2);
}

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
  // If --enable-panels is on, always use panels on Linux.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnablePanels))
    return true;

  // Otherwise, panels are only supported on tested window managers.
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
  if (win8::IsSingleWindowMetroMode())
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

// static
bool PanelManager::IsPanelStackingEnabled() {
#if defined(OS_WIN)
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePanelStacking);
#else
  return false;
#endif
}

PanelManager::PanelManager()
    : panel_mouse_watcher_(PanelMouseWatcher::Create()),
      auto_sizing_enabled_(true) {
  // DisplaySettingsProvider should be created before the creation of
  // collections since some collection might depend on it.
  display_settings_provider_.reset(DisplaySettingsProvider::Create());
  display_settings_provider_->AddDisplayAreaObserver(this);

  detached_collection_.reset(new DetachedPanelCollection(this));
  docked_collection_.reset(new DockedPanelCollection(this));
  drag_controller_.reset(new PanelDragController(this));
  resize_controller_.reset(new PanelResizeController(this));
}

PanelManager::~PanelManager() {
  display_settings_provider_->RemoveDisplayAreaObserver(this);

  // Docked collection should be disposed explicitly before
  // DisplaySettingsProvider is gone since docked collection needs to remove
  // the observer from DisplaySettingsProvider.
  docked_collection_.reset();
}

gfx::Point PanelManager::GetDefaultDetachedPanelOrigin() {
  return detached_collection_->GetDefaultPanelOrigin();
}

void PanelManager::OnDisplayAreaChanged(const gfx::Rect& display_area) {
  if (display_area == display_area_)
    return;
  gfx::Rect old_display_area = display_area_;
  display_area_ = display_area;

  docked_collection_->OnDisplayAreaChanged(old_display_area);
  detached_collection_->OnDisplayAreaChanged(old_display_area);
  for (Stacks::const_iterator iter = stacks_.begin();
       iter != stacks_.end(); iter++)
    (*iter)->OnDisplayAreaChanged(old_display_area);
}

void PanelManager::OnFullScreenModeChanged(bool is_full_screen) {
  docked_collection_->OnFullScreenModeChanged(is_full_screen);
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
    bounds.set_origin(
        docked_collection_->GetDefaultPositionForPanel(bounds.size()));
  } else {
    bounds.set_x(requested_bounds.x());
    bounds.set_y(IsPanelStackingEnabled() ?
        display_area_.y() + kDetachedPanelStartingYPositionOnStackingEnabled :
        requested_bounds.y());
    bounds.AdjustToFit(display_settings_provider_->GetDisplayArea());
  }

  // Create the panel.
  Panel* panel = new Panel(app_name, min_size, max_size);
  panel->Initialize(profile, url, bounds);

  // Auto resizable feature is enabled only if no initial size is requested.
  if (auto_sizing_enabled() && requested_bounds.width() == 0 &&
      requested_bounds.height() == 0) {
    panel->SetAutoResizable(true);
  }

  // Add the panel to the appropriate panel collection.
  PanelCollection::PositioningMask positioning_mask;
  PanelCollection* collection = GetCollectionForNewPanel(
      panel, bounds, mode, &positioning_mask);
  collection->AddPanel(panel, positioning_mask);
  collection->UpdatePanelOnCollectionChange(panel);

  return panel;
}

PanelCollection* PanelManager::GetCollectionForNewPanel(
    Panel* new_panel,
    const gfx::Rect& bounds,
    CreateMode mode,
    PanelCollection::PositioningMask* positioning_mask) {
  if (mode == CREATE_AS_DOCKED) {
    // Delay layout refreshes in case multiple panels are created within
    // a short time of one another or the focus changes shortly after panel
    // is created to avoid excessive screen redraws.
    *positioning_mask = PanelCollection::DELAY_LAYOUT_REFRESH;
    return docked_collection_.get();
  }

  DCHECK_EQ(CREATE_AS_DETACHED, mode);
  *positioning_mask = PanelCollection::DEFAULT_POSITION;

  // If the stacking support is not enabled, new panel will still be created as
  // detached.
  if (!IsPanelStackingEnabled())
    return detached_collection_.get();

  // If there're stacks, try to find a stack that can fit new panel.
  if (!stacks_.empty()) {
    // Perform the search as:
    // 1) Search from the stack with more panels to the stack with least panels.
    // 2) Amongs the stacks with same number of panels, search from the right-
    //    most stack to the left-most stack.
    // 3) Among the stack with same number of panels and same x position,
    //    search from the top-most stack to the bottom-most stack.
    // 4) If there is not enough space to fit new panel even with all inactive
    //    panels being collapsed, move to next stack.
    stacks_.sort(ComparerNumberOfPanelsInStack);
    for (Stacks::const_iterator iter = stacks_.begin();
         iter != stacks_.end(); iter++) {
      StackedPanelCollection* stack = *iter;

      // Do not add to other stack that is from differnt extension or profile.
      // Note that the check is based on bottom panel.
      Panel* panel = stack->bottom_panel();
      if (panel->profile() != new_panel->profile() ||
          panel->extension_id() != new_panel->extension_id())
        continue;

      if (bounds.height() <= stack->GetMaximiumAvailableBottomSpace()) {
        *positioning_mask = static_cast<PanelCollection::PositioningMask>(
            *positioning_mask | PanelCollection::COLLAPSE_TO_FIT);
        return stack;
      }
    }
  }

  // Then try to find a detached panel to which new panel can stack.
  if (detached_collection_->num_panels()) {
    // Perform the search as:
    // 1) Search from the right-most detached panel to the left-most detached
    //    panel.
    // 2) Among the detached panels with same x position, search from the
    //    top-most detached panel to the bottom-most deatched panel.
    // 3) If there is not enough space beneath the detached panel, even by
    //    collapsing it if it is inactive, to fit new panel, move to next
    //    detached panel.
    detached_collection_->SortPanels(CompareDetachedPanels);

    for (DetachedPanelCollection::Panels::const_iterator iter =
             detached_collection_->panels().begin();
         iter != detached_collection_->panels().end(); ++iter) {
      Panel* panel = *iter;

      // Do not stack with other panel that is from differnt extension or
      // profile.
      if (panel->profile() != new_panel->profile() ||
          panel->extension_id() != new_panel->extension_id())
        continue;

      int max_available_space =
          display_area_.bottom() - panel->GetBounds().y() -
          (panel->IsActive() ? panel->GetBounds().height()
                             : panel::kTitlebarHeight);
      if (bounds.height() <= max_available_space) {
        StackedPanelCollection* new_stack = CreateStack();
        MovePanelToCollection(panel,
                              new_stack,
                              PanelCollection::DEFAULT_POSITION);
        *positioning_mask = static_cast<PanelCollection::PositioningMask>(
            *positioning_mask | PanelCollection::COLLAPSE_TO_FIT);
        return new_stack;
      }
    }
  }

  return detached_collection_.get();
}

void PanelManager::OnPanelClosed(Panel* panel) {
  if (num_panels() == 1) {
    display_settings_provider_->RemoveFullScreenObserver(this);
  }

  drag_controller_->OnPanelClosed(panel);
  resize_controller_->OnPanelClosed(panel);

  // Note that we need to keep track of panel's collection since it will be
  // gone once RemovePanel is called.
  PanelCollection* collection = panel->collection();
  collection->RemovePanel(panel, PanelCollection::PANEL_CLOSED);

  // If only one panel is left in the stack, move it out of the stack.
  // Also make sure that this detached panel will be expanded if not yet.
  if (collection->type() == PanelCollection::STACKED) {
    StackedPanelCollection* stack =
        static_cast<StackedPanelCollection*>(collection);
    DCHECK_GE(stack->num_panels(), 1);
    if (stack->num_panels() == 1) {
      Panel* top_panel = stack->top_panel();
      MovePanelToCollection(top_panel,
                            detached_collection(),
                            PanelCollection::DEFAULT_POSITION);
      if (top_panel->expansion_state() != Panel::EXPANDED)
        top_panel->SetExpansionState(Panel::EXPANDED);
      RemoveStack(stack);
    }
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_CLOSED,
      content::Source<Panel>(panel),
      content::NotificationService::NoDetails());
}

StackedPanelCollection* PanelManager::CreateStack() {
  StackedPanelCollection* stack = new StackedPanelCollection(this);
  stacks_.push_back(stack);
  return stack;
}

void PanelManager::RemoveStack(StackedPanelCollection* stack) {
  DCHECK_EQ(0, stack->num_panels());
  stacks_.remove(stack);
  stack->CloseAll();
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
    if (!cancelled && resized_panel->collection())
      resized_panel->collection()->RefreshLayout();
  }
}

void PanelManager::OnPanelExpansionStateChanged(Panel* panel) {
  panel->collection()->OnPanelExpansionStateChanged(panel);
}

void PanelManager::MovePanelToCollection(
    Panel* panel,
    PanelCollection* target_collection,
    PanelCollection::PositioningMask positioning_mask) {
  DCHECK(panel);
  PanelCollection* current_collection = panel->collection();
  DCHECK(current_collection);
  DCHECK_NE(current_collection, target_collection);
  current_collection->RemovePanel(panel,
                                  PanelCollection::PANEL_CHANGED_COLLECTION);

  target_collection->AddPanel(panel, positioning_mask);
  target_collection->UpdatePanelOnCollectionChange(panel);
}

bool PanelManager::ShouldBringUpTitlebars(int mouse_x, int mouse_y) const {
  return docked_collection_->ShouldBringUpTitlebars(mouse_x, mouse_y);
}

void PanelManager::BringUpOrDownTitlebars(bool bring_up) {
  docked_collection_->BringUpOrDownTitlebars(bring_up);
}

void PanelManager::CloseAll() {
  DCHECK(!drag_controller_->is_dragging());

  detached_collection_->CloseAll();
  docked_collection_->CloseAll();
}

int PanelManager::num_panels() const {
  int count = detached_collection_->num_panels() +
              docked_collection_->num_panels();
  for (Stacks::const_iterator iter = stacks_.begin();
       iter != stacks_.end(); iter++)
    count += (*iter)->num_panels();
  return count;
}

std::vector<Panel*> PanelManager::panels() const {
  std::vector<Panel*> panels;
  for (DetachedPanelCollection::Panels::const_iterator iter =
           detached_collection_->panels().begin();
       iter != detached_collection_->panels().end(); ++iter)
    panels.push_back(*iter);
  for (DockedPanelCollection::Panels::const_iterator iter =
           docked_collection_->panels().begin();
       iter != docked_collection_->panels().end(); ++iter)
    panels.push_back(*iter);
  for (Stacks::const_iterator stack_iter = stacks_.begin();
       stack_iter != stacks_.end(); stack_iter++) {
    for (StackedPanelCollection::Panels::const_iterator iter =
             (*stack_iter)->panels().begin();
         iter != (*stack_iter)->panels().end(); ++stack_iter) {
      panels.push_back(*iter);
    }
  }
  return panels;
}

std::vector<Panel*> PanelManager::GetDetachedAndStackedPanels() const {
  std::vector<Panel*> panels;
  for (DetachedPanelCollection::Panels::const_iterator iter =
           detached_collection_->panels().begin();
       iter != detached_collection_->panels().end(); ++iter)
    panels.push_back(*iter);
  for (Stacks::const_iterator stack_iter = stacks_.begin();
       stack_iter != stacks_.end(); stack_iter++) {
    for (StackedPanelCollection::Panels::const_iterator iter =
             (*stack_iter)->panels().begin();
         iter != (*stack_iter)->panels().end(); ++iter) {
      panels.push_back(*iter);
    }
  }
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
