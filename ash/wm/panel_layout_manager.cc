// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panel_layout_manager.h"

#include <algorithm>
#include <map>

#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/frame_painter.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {
const int kPanelMarginEdge = 4;
const int kPanelMarginMiddle = 8;
const int kPanelIdealSpacing = 4;

const float kMaxHeightFactor = .80f;
const float kMaxWidthFactor = .50f;

// Callout arrow dimensions.
const int kArrowWidth = 20;
const int kArrowHeight = 10;

class CalloutWidgetBackground : public views::Background {
 public:
  CalloutWidgetBackground() {}
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    SkPath path;
    // TODO(dcheng): Verify if this results in off by one errors.
    path.moveTo(SkIntToScalar(0), SkIntToScalar(0));
    path.lineTo(SkIntToScalar(kArrowWidth / 2), SkIntToScalar(kArrowHeight));
    path.lineTo(SkIntToScalar(kArrowWidth), SkIntToScalar(0));

    // Use the same opacity and colors as the header for now.
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(
        SkColorSetARGB(FramePainter::kActiveWindowOpacity, 189, 189, 189));
    canvas->DrawPath(path, paint);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CalloutWidgetBackground);
};

struct VisiblePanelPositionInfo {
  VisiblePanelPositionInfo()
      : min_x(0),
        max_x(0),
        x(0),
        width(0),
        window(NULL) {}

  int min_x;
  int max_x;
  int x;
  int width;
  aura::Window* window;
};

bool CompareWindowX(const VisiblePanelPositionInfo& win1,
                    const VisiblePanelPositionInfo& win2) {
  return win1.x < win2.x;
}

void FanOutPanels(std::vector<VisiblePanelPositionInfo>::iterator first,
                  std::vector<VisiblePanelPositionInfo>::iterator last) {
  int num_panels = last - first;
  if (num_panels <= 1)
    return;

  if (num_panels == 2) {
    // If there are two adjacent overlapping windows, separate them by the
    // minimum width necessary.
    std::vector<VisiblePanelPositionInfo>::iterator second = first + 1;
    int separation = (*first).width + kPanelIdealSpacing;
    int overlap = (*first).x + separation - (*second).x;
    (*first).x = std::max((*first).min_x, (*first).x - overlap / 2);
    (*second).x = std::min((*second).max_x, (*first).x + separation);
    // Recalculate the first panel position in case the second one was
    // constrained on the right.
    (*first).x = std::max((*first).min_x, (*second).x - separation);
    return;
  }

  // If there are more than two overlapping windows, fan them out from minimum
  // position to maximum position equally spaced.
  int delta = ((*(last - 1)).max_x - (*first).min_x) / (num_panels - 1);
  int x = (*first).min_x;
  for (std::vector<VisiblePanelPositionInfo>::iterator iter = first;
      iter != last; ++iter) {
    (*iter).x = std::max((*iter).min_x, std::min((*iter).max_x, x));
    x += delta;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PanelLayoutManager public implementation:
PanelLayoutManager::PanelLayoutManager(aura::Window* panel_container)
    : panel_container_(panel_container),
      in_layout_(false),
      dragged_panel_(NULL),
      launcher_(NULL),
      last_active_panel_(NULL),
      callout_widget_(new views::Widget),
      weak_factory_(this) {
  DCHECK(panel_container);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.transparent = true;
  params.can_activate = false;
  params.keep_on_top = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = panel_container_;
  params.bounds = ScreenAsh::ConvertRectToScreen(panel_container_, gfx::Rect());
  params.bounds.set_width(kArrowWidth);
  params.bounds.set_height(kArrowHeight);
  // Why do we need this and can_activate = false?
  callout_widget_->set_focus_on_creation(false);
  callout_widget_->Init(params);
  DCHECK_EQ(callout_widget_->GetNativeView()->GetRootWindow(),
            panel_container_->GetRootWindow());
  views::View* content_view = new views::View;
  content_view->set_background(new CalloutWidgetBackground);
  callout_widget_->SetContentsView(content_view);
  aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      AddObserver(this);
}

PanelLayoutManager::~PanelLayoutManager() {
  if (launcher_)
    launcher_->RemoveIconObserver(this);
  aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      RemoveObserver(this);
}

void PanelLayoutManager::Shutdown() {
  callout_widget_.reset();
}

void PanelLayoutManager::StartDragging(aura::Window* panel) {
  DCHECK(!dragged_panel_);
  DCHECK(panel->parent() == panel_container_);
  dragged_panel_ = panel;
  Relayout();
}

void PanelLayoutManager::FinishDragging() {
  // Note, dragged panel may be null if the panel was just attached to the
  // panel layout.
  dragged_panel_ = NULL;
  Relayout();
}

void PanelLayoutManager::SetLauncher(ash::Launcher* launcher) {
  launcher_ = launcher;
  launcher_->AddIconObserver(this);
}

void PanelLayoutManager::ToggleMinimize(aura::Window* panel) {
  DCHECK(panel->parent() == panel_container_);
  if (panel->GetProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MINIMIZED) {
    panel->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  } else {
    panel->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  }
}

////////////////////////////////////////////////////////////////////////////////
// PanelLayoutManager, aura::LayoutManager implementation:
void PanelLayoutManager::OnWindowResized() {
  Relayout();
}

void PanelLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  if (callout_widget_ && child == callout_widget_->GetNativeWindow())
    return;
  panel_windows_.push_back(child);
  child->AddObserver(this);
  Relayout();
}

void PanelLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
}

void PanelLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
  PanelList::iterator found =
      std::find(panel_windows_.begin(), panel_windows_.end(), child);
  if (found != panel_windows_.end())
    panel_windows_.erase(found);
  child->RemoveObserver(this);

  if (dragged_panel_ == child)
    dragged_panel_ = NULL;

  if (last_active_panel_ == child)
    last_active_panel_ = NULL;

  Relayout();
}

void PanelLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                        bool visible) {
  Relayout();
}

void PanelLayoutManager::SetChildBounds(aura::Window* child,
                                        const gfx::Rect& requested_bounds) {
  gfx::Rect bounds(requested_bounds);
  const gfx::Rect& max_bounds = panel_container_->GetRootWindow()->bounds();
  const int max_width = max_bounds.width() * kMaxWidthFactor;
  const int max_height = max_bounds.height() * kMaxHeightFactor;
  if (bounds.width() > max_width)
    bounds.set_width(max_width);
  if (bounds.height() > max_height)
    bounds.set_height(max_height);

  // Reposition dragged panel in the panel order.
  if (dragged_panel_ == child) {
    PanelList::iterator dragged_panel_iter =
      std::find(panel_windows_.begin(), panel_windows_.end(), dragged_panel_);
    DCHECK(dragged_panel_iter != panel_windows_.end());
    PanelList::iterator new_position;
    for (new_position = panel_windows_.begin();
         new_position != panel_windows_.end();
         ++new_position) {
      const gfx::Rect& bounds = (*new_position)->bounds();
      if (bounds.x() + bounds.width()/2 <= requested_bounds.x()) break;
    }
    if (new_position != dragged_panel_iter) {
      panel_windows_.erase(dragged_panel_iter);
      panel_windows_.insert(new_position, dragged_panel_);
    }
  }

  SetChildBoundsDirect(child, bounds);
  Relayout();
}

////////////////////////////////////////////////////////////////////////////////
// PanelLayoutManager, ash::LauncherIconObserver implementation:

void PanelLayoutManager::OnLauncherIconPositionsChanged() {
  // TODO: As this is called for every animation step now. Relayout needs to be
  // updated to use current icon position instead of use the ideal bounds so
  // that the panels slide with their icons instead of jumping.
  Relayout();
}

/////////////////////////////////////////////////////////////////////////////
// PanelLayoutManager, WindowObserver implementation:

void PanelLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                 const void* key,
                                                 intptr_t old) {
  if (key != aura::client::kShowStateKey)
    return;
  ui::WindowShowState new_state =
      window->GetProperty(aura::client::kShowStateKey);
  if (new_state == ui::SHOW_STATE_MINIMIZED)
    MinimizePanel(window);
  else
    RestorePanel(window);
}

void PanelLayoutManager::OnWindowVisibilityChanged(
    aura::Window* window, bool visible) {
  if (visible)
    window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
}

////////////////////////////////////////////////////////////////////////////////
// PanelLayoutManager, aura::client::ActivationChangeObserver implementation:

void PanelLayoutManager::OnWindowActivated(aura::Window* gained_active,
                                           aura::Window* lost_active) {
  // Ignore if the panel that is not managed by this was activated.
  if (gained_active &&
      gained_active->type() == aura::client::WINDOW_TYPE_PANEL &&
      gained_active->parent() == panel_container_) {
    UpdateStacking(gained_active);
    UpdateCallout(gained_active);
  } else {
    UpdateCallout(NULL);
  }
}

////////////////////////////////////////////////////////////////////////////////
// PanelLayoutManager private implementation:

void PanelLayoutManager::MinimizePanel(aura::Window* panel) {
  views::corewm::SetWindowVisibilityAnimationType(
      panel, WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);
  panel->Hide();
  if (wm::IsActiveWindow(panel))
    wm::DeactivateWindow(panel);
  Relayout();
}

void PanelLayoutManager::RestorePanel(aura::Window* panel) {
  panel->Show();
  Relayout();
}

void PanelLayoutManager::Relayout() {
  if (!launcher_ || !launcher_->widget())
    return;

  if (in_layout_)
    return;
  base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);

  int launcher_top = launcher_->widget()->GetWindowBoundsInScreen().y();
  int panel_left_bounds = kPanelIdealSpacing;
  int panel_right_bounds =
      panel_container_->bounds().width() - kPanelIdealSpacing;
  aura::Window* active_panel = NULL;
  std::vector<VisiblePanelPositionInfo> visible_panels;
  for (PanelList::iterator iter = panel_windows_.begin();
       iter != panel_windows_.end(); ++iter) {
    aura::Window* panel = *iter;
    if (!panel->IsVisible() || panel == dragged_panel_)
      continue;

    gfx::Rect icon_bounds =
        launcher_->GetScreenBoundsOfItemIconForWindow(panel);

    // An empty rect indicates that there is no icon for the panel in the
    // launcher. Just use the current bounds, as there's no icon to draw the
    // panel above.
    // TODO(dcheng): Need to anchor to overflow icon.
    if (icon_bounds.IsEmpty())
      continue;

    if (panel->HasFocus() ||
        panel->Contains(
            aura::client::GetFocusClient(panel)->GetFocusedWindow())) {
      DCHECK(!active_panel);
      active_panel = panel;
    }
    icon_bounds = ScreenAsh::ConvertRectFromScreen(panel_container_,
                                                   icon_bounds);
    gfx::Point icon_origin = icon_bounds.origin();
    VisiblePanelPositionInfo position_info;
    position_info.min_x = std::max(panel_left_bounds, icon_origin.x() +
        icon_bounds.width() - panel->bounds().width());
    position_info.max_x = std::min(icon_origin.x(),
                                   panel_right_bounds - kPanelIdealSpacing -
                                   panel->bounds().width());
    position_info.x = icon_origin.x() + icon_bounds.width() / 2 -
                      panel->bounds().width() / 2;
    position_info.width = panel->bounds().width();
    position_info.window = panel;
    visible_panels.push_back(position_info);
  }

  // Sort panels by their X positions and fan out groups of overlapping panels.
  // The fan out method may result in new overlapping panels however given that
  // the panels start at least a full panel width apart this overlap will
  // never completely obscure a panel.
  // TODO(flackr): Rearrange panels if new overlaps are introduced.
  std::sort(visible_panels.begin(), visible_panels.end(), CompareWindowX);
  size_t first_overlapping_panel = 0;
  for (size_t i = 1; i < visible_panels.size(); ++i) {
    if (visible_panels[i - 1].x + visible_panels[i - 1].width
        < visible_panels[i].x) {
      FanOutPanels(visible_panels.begin() + first_overlapping_panel,
                   visible_panels.begin() + i);
      first_overlapping_panel = i;
    }
  }
  FanOutPanels(visible_panels.begin() + first_overlapping_panel,
               visible_panels.end());

  for (size_t i = 0; i < visible_panels.size(); ++i) {
    gfx::Rect bounds = visible_panels[i].window->bounds();
    bounds.set_x(visible_panels[i].x);
    bounds.set_y(launcher_top - bounds.height());
    SetChildBoundsDirect(visible_panels[i].window, bounds);
  }

  UpdateStacking(active_panel);
  UpdateCallout(active_panel);
}

void PanelLayoutManager::UpdateStacking(aura::Window* active_panel) {
  if (!active_panel) {
    if (!last_active_panel_)
      return;
    active_panel = last_active_panel_;
  }

  // We want to to stack the panels like a deck of cards:
  // ,--,--,--,-------.--.--.
  // |  |  |  |       |  |  |
  // |  |  |  |       |  |  |
  //
  // We use the middle of each panel to figure out how to stack the panels. This
  // allows us to update the stacking when a panel is being dragged around by
  // the titlebar--even though it doesn't update the launcher icon positions, we
  // still want the visual effect.
  std::map<int, aura::Window*> window_ordering;
  for (PanelList::const_iterator it = panel_windows_.begin();
       it != panel_windows_.end(); ++it) {
    gfx::Rect bounds = (*it)->bounds();
    window_ordering.insert(std::make_pair(bounds.x() + bounds.width() / 2,
                                          *it));
  }

  aura::Window* previous_panel = NULL;
  for (std::map<int, aura::Window*>::const_iterator it =
       window_ordering.begin();
       it != window_ordering.end() && it->second != active_panel; ++it) {
    if (previous_panel)
      panel_container_->StackChildAbove(it->second, previous_panel);
    previous_panel = it->second;
  }

  previous_panel = NULL;
  for (std::map<int, aura::Window*>::const_reverse_iterator it =
       window_ordering.rbegin();
       it != window_ordering.rend() && it->second != active_panel; ++it) {
    if (previous_panel)
      panel_container_->StackChildAbove(it->second, previous_panel);
    previous_panel = it->second;
  }

  panel_container_->StackChildAtTop(active_panel);
  last_active_panel_ = active_panel;
}

void PanelLayoutManager::UpdateCallout(aura::Window* active_panel) {
  weak_factory_.InvalidateWeakPtrs();
  // TODO(dcheng): This doesn't account for panels in overflow. They should have
  // a callout as well.
  if (!active_panel ||
      launcher_->GetScreenBoundsOfItemIconForWindow(active_panel).IsEmpty()) {
    if (callout_widget_)
      callout_widget_->Hide();
    return;
  }
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&PanelLayoutManager::ShowCalloutHelper,
                 weak_factory_.GetWeakPtr(),
                 active_panel));
}

void PanelLayoutManager::ShowCalloutHelper(aura::Window* active_panel) {
  if (!callout_widget_)
    return;
  DCHECK(active_panel);
  gfx::Rect bounds = active_panel->GetBoundsInRootWindow();
  gfx::Rect icon_bounds =
      launcher_->GetScreenBoundsOfItemIconForWindow(active_panel);
  gfx::Rect callout_bounds = callout_widget_->GetWindowBoundsInScreen();
  callout_bounds.set_x(
      icon_bounds.x() + (icon_bounds.width() - callout_bounds.width()) / 2);
  callout_bounds.set_y(bounds.bottom());
  callout_bounds = ScreenAsh::ConvertRectFromScreen(
      callout_widget_->GetNativeWindow()->parent(),
      callout_bounds);

  SetChildBoundsDirect(callout_widget_->GetNativeWindow(), callout_bounds);
  panel_container_->StackChildAtTop(callout_widget_->GetNativeWindow());
  callout_widget_->Show();
}

}  // namespace internal
}  // namespace ash
