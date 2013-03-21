// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panels/panel_layout_manager.h"

#include <algorithm>
#include <map>

#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
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
  CalloutWidgetBackground() : alignment_(SHELF_ALIGNMENT_BOTTOM) {
  }

  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    SkPath path;
    switch (alignment_) {
      case SHELF_ALIGNMENT_BOTTOM:
        path.moveTo(SkIntToScalar(0), SkIntToScalar(0));
        path.lineTo(SkIntToScalar(kArrowWidth / 2),
                    SkIntToScalar(kArrowHeight));
        path.lineTo(SkIntToScalar(kArrowWidth), SkIntToScalar(0));
        break;
      case SHELF_ALIGNMENT_LEFT:
        path.moveTo(SkIntToScalar(kArrowHeight), SkIntToScalar(kArrowWidth));
        path.lineTo(SkIntToScalar(0), SkIntToScalar(kArrowWidth / 2));
        path.lineTo(SkIntToScalar(kArrowHeight), SkIntToScalar(0));
        break;
      case SHELF_ALIGNMENT_TOP:
        path.moveTo(SkIntToScalar(0), SkIntToScalar(kArrowHeight));
        path.lineTo(SkIntToScalar(kArrowWidth / 2), SkIntToScalar(0));
        path.lineTo(SkIntToScalar(kArrowWidth), SkIntToScalar(kArrowHeight));
        break;
      case SHELF_ALIGNMENT_RIGHT:
        path.moveTo(SkIntToScalar(0), SkIntToScalar(0));
        path.lineTo(SkIntToScalar(kArrowHeight),
                    SkIntToScalar(kArrowWidth / 2));
        path.lineTo(SkIntToScalar(0), SkIntToScalar(kArrowWidth));
        break;
    }
    // Use the same opacity and colors as the header for now.
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(
        SkColorSetARGB(FramePainter::kActiveWindowOpacity, 189, 189, 189));
    canvas->DrawPath(path, paint);
  }

  void set_alignment(ShelfAlignment alignment) {
    alignment_ = alignment;
  }

 private:
  ShelfAlignment alignment_;

  DISALLOW_COPY_AND_ASSIGN(CalloutWidgetBackground);
};

struct VisiblePanelPositionInfo {
  VisiblePanelPositionInfo()
      : min_major(0),
        max_major(0),
        major_pos(0),
        major_length(0),
        window(NULL) {}

  int min_major;
  int max_major;
  int major_pos;
  int major_length;
  aura::Window* window;
};

bool CompareWindowMajor(const VisiblePanelPositionInfo& win1,
                        const VisiblePanelPositionInfo& win2) {
  return win1.major_pos < win2.major_pos;
}

void FanOutPanels(std::vector<VisiblePanelPositionInfo>::iterator first,
                  std::vector<VisiblePanelPositionInfo>::iterator last) {
  int num_panels = last - first;
  if (num_panels <= 1)
    return;

  if (num_panels == 2) {
    // If there are two adjacent overlapping windows, separate them by the
    // minimum major_length necessary.
    std::vector<VisiblePanelPositionInfo>::iterator second = first + 1;
    int separation = (*first).major_length / 2 + (*second).major_length / 2 +
                     kPanelIdealSpacing;
    int overlap = (*first).major_pos + separation - (*second).major_pos;
    (*first).major_pos = std::max((*first).min_major,
                                  (*first).major_pos - overlap / 2);
    (*second).major_pos = std::min((*second).max_major,
                                   (*first).major_pos + separation);
    // Recalculate the first panel position in case the second one was
    // constrained on the right.
    (*first).major_pos = std::max((*first).min_major,
                                  (*second).major_pos - separation);
    return;
  }

  // If there are more than two overlapping windows, fan them out from minimum
  // position to maximum position equally spaced.
  int delta = ((*(last - 1)).max_major - (*first).min_major) / (num_panels - 1);
  int major_pos = (*first).min_major;
  for (std::vector<VisiblePanelPositionInfo>::iterator iter = first;
      iter != last; ++iter) {
    (*iter).major_pos = std::max((*iter).min_major,
                                 std::min((*iter).max_major, major_pos));
    major_pos += delta;
  }
}

}  // namespace

class PanelCalloutWidget : public views::Widget {
 public:
  explicit PanelCalloutWidget(aura::Window* container)
      : background_(NULL) {
    InitWidget(container);
  }

  void SetAlignment(ShelfAlignment alignment) {
    gfx::Rect callout_bounds = GetWindowBoundsInScreen();
    if (alignment == SHELF_ALIGNMENT_BOTTOM ||
        alignment == SHELF_ALIGNMENT_TOP) {
      callout_bounds.set_width(kArrowWidth);
      callout_bounds.set_height(kArrowHeight);
    } else {
      callout_bounds.set_width(kArrowHeight);
      callout_bounds.set_height(kArrowWidth);
    }
    GetNativeWindow()->SetBounds(callout_bounds);
    background_->set_alignment(alignment);
  }

 private:
  void InitWidget(aura::Window* parent) {
    views::Widget::InitParams params;
    params.type = views::Widget::InitParams::TYPE_POPUP;
    params.transparent = true;
    params.can_activate = false;
    params.keep_on_top = true;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.parent = parent;
    params.bounds = ScreenAsh::ConvertRectToScreen(parent, gfx::Rect());
    params.bounds.set_width(kArrowWidth);
    params.bounds.set_height(kArrowHeight);
    // Why do we need this and can_activate = false?
    set_focus_on_creation(false);
    Init(params);
    DCHECK_EQ(GetNativeView()->GetRootWindow(), parent->GetRootWindow());
    views::View* content_view = new views::View;
    background_ = new CalloutWidgetBackground;
    content_view->set_background(background_);
    SetContentsView(content_view);
  }

  // Weak pointer owned by this widget's content view.
  CalloutWidgetBackground* background_;

  DISALLOW_COPY_AND_ASSIGN(PanelCalloutWidget);
};

////////////////////////////////////////////////////////////////////////////////
// PanelLayoutManager public implementation:
PanelLayoutManager::PanelLayoutManager(aura::Window* panel_container)
    : panel_container_(panel_container),
      in_layout_(false),
      dragged_panel_(NULL),
      launcher_(NULL),
      last_active_panel_(NULL),
      weak_factory_(this) {
  DCHECK(panel_container);
  aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      AddObserver(this);
  Shell::GetInstance()->AddShellObserver(this);
}

PanelLayoutManager::~PanelLayoutManager() {
  Shutdown();
  aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      RemoveObserver(this);
  Shell::GetInstance()->RemoveShellObserver(this);
}

void PanelLayoutManager::Shutdown() {
  for (PanelList::iterator iter = panel_windows_.begin();
       iter != panel_windows_.end(); ++iter) {
    delete iter->callout_widget;
  }
  panel_windows_.clear();
  if (launcher_)
    launcher_->RemoveIconObserver(this);
  launcher_ = NULL;
}

void PanelLayoutManager::StartDragging(aura::Window* panel) {
  DCHECK(!dragged_panel_);
  dragged_panel_ = panel;
  Relayout();
}

void PanelLayoutManager::FinishDragging() {
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
  if (child->type() == aura::client::WINDOW_TYPE_POPUP)
    return;
  PanelInfo panel_info;
  panel_info.window = child;
  panel_info.callout_widget = new PanelCalloutWidget(panel_container_);
  panel_windows_.push_back(panel_info);
  child->AddObserver(this);
  Relayout();
}

void PanelLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
}

void PanelLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
  if (child->type() == aura::client::WINDOW_TYPE_POPUP)
    return;
  PanelList::iterator found =
      std::find(panel_windows_.begin(), panel_windows_.end(), child);
  if (found != panel_windows_.end()) {
    delete found->callout_widget;
    panel_windows_.erase(found);
  }
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
      const gfx::Rect& bounds = (*new_position).window->bounds();
      if (bounds.x() + bounds.width()/2 <= requested_bounds.x()) break;
    }
    if (new_position != dragged_panel_iter) {
      PanelInfo dragged_panel_info = *dragged_panel_iter;
      panel_windows_.erase(dragged_panel_iter);
      panel_windows_.insert(new_position, dragged_panel_info);
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

////////////////////////////////////////////////////////////////////////////////
// PanelLayoutManager, ash::ShellObserver implementation:

void PanelLayoutManager::OnShelfAlignmentChanged(
    aura::RootWindow* root_window) {
  if (panel_container_->GetRootWindow() == root_window)
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
    UpdateCallouts();
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
  if (!launcher_ || !launcher_->shelf_widget())
    return;

  if (in_layout_)
    return;
  base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);

  ShelfAlignment alignment = launcher_->shelf_widget()->GetAlignment();
  bool horizontal = alignment == SHELF_ALIGNMENT_TOP ||
                    alignment == SHELF_ALIGNMENT_BOTTOM;
  gfx::Rect launcher_bounds = launcher_->shelf_widget()->
      GetWindowBoundsInScreen();
  int panel_start_bounds = kPanelIdealSpacing;
  int panel_end_bounds = horizontal ?
      panel_container_->bounds().width() - kPanelIdealSpacing :
      panel_container_->bounds().height() - kPanelIdealSpacing;
  aura::Window* active_panel = NULL;
  std::vector<VisiblePanelPositionInfo> visible_panels;
  for (PanelList::iterator iter = panel_windows_.begin();
       iter != panel_windows_.end(); ++iter) {
    aura::Window* panel = iter->window;
    iter->callout_widget->SetAlignment(alignment);
    if (!panel->IsVisible() || panel == dragged_panel_)
      continue;

    gfx::Rect icon_bounds =
        launcher_->GetScreenBoundsOfItemIconForWindow(panel);

    // An empty rect indicates that there is no icon for the panel in the
    // launcher. Just use the current bounds, as there's no icon to draw the
    // panel above.
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
    int icon_start = horizontal ? icon_origin.x() : icon_origin.y();
    int icon_end = icon_start + (horizontal ? icon_bounds.width() :
                                 icon_bounds.height());
    position_info.major_length = horizontal ?
        panel->bounds().width() : panel->bounds().height();
    position_info.min_major = std::max(
        panel_start_bounds + position_info.major_length / 2,
        icon_end - position_info.major_length / 2);
    position_info.max_major = std::min(
        icon_start + position_info.major_length / 2,
        panel_end_bounds - position_info.major_length / 2);
    position_info.major_pos = (icon_start + icon_end) / 2;
    position_info.window = panel;
    visible_panels.push_back(position_info);
  }

  // Sort panels by their X positions and fan out groups of overlapping panels.
  // The fan out method may result in new overlapping panels however given that
  // the panels start at least a full panel width apart this overlap will
  // never completely obscure a panel.
  // TODO(flackr): Rearrange panels if new overlaps are introduced.
  std::sort(visible_panels.begin(), visible_panels.end(), CompareWindowMajor);
  size_t first_overlapping_panel = 0;
  for (size_t i = 1; i < visible_panels.size(); ++i) {
    if (visible_panels[i - 1].major_pos +
        visible_panels[i - 1].major_length / 2 < visible_panels[i].major_pos -
        visible_panels[i].major_length / 2) {
      FanOutPanels(visible_panels.begin() + first_overlapping_panel,
                   visible_panels.begin() + i);
      first_overlapping_panel = i;
    }
  }
  FanOutPanels(visible_panels.begin() + first_overlapping_panel,
               visible_panels.end());

  for (size_t i = 0; i < visible_panels.size(); ++i) {
    gfx::Rect bounds = visible_panels[i].window->bounds();
    if (horizontal)
      bounds.set_x(visible_panels[i].major_pos -
                   visible_panels[i].major_length / 2);
    else
      bounds.set_y(visible_panels[i].major_pos -
                   visible_panels[i].major_length / 2);
    switch (alignment) {
      case SHELF_ALIGNMENT_BOTTOM:
        bounds.set_y(launcher_bounds.y() - bounds.height());
        break;
      case SHELF_ALIGNMENT_LEFT:
        bounds.set_x(launcher_bounds.right());
        break;
      case SHELF_ALIGNMENT_RIGHT:
        bounds.set_x(launcher_bounds.x() - bounds.width());
        break;
      case SHELF_ALIGNMENT_TOP:
        bounds.set_y(launcher_bounds.bottom());
        break;
    }
    SetChildBoundsDirect(visible_panels[i].window, bounds);
  }

  UpdateStacking(active_panel);
  UpdateCallouts();
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
    gfx::Rect bounds = it->window->bounds();
    window_ordering.insert(std::make_pair(bounds.x() + bounds.width() / 2,
                                          it->window));
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
  if (dragged_panel_ && dragged_panel_->parent() == panel_container_)
    panel_container_->StackChildAtTop(dragged_panel_);
  last_active_panel_ = active_panel;
}

void PanelLayoutManager::UpdateCallouts() {
  ShelfAlignment alignment = launcher_->alignment();
  bool horizontal = alignment == SHELF_ALIGNMENT_TOP ||
                    alignment == SHELF_ALIGNMENT_BOTTOM;

  for (PanelList::iterator iter = panel_windows_.begin();
       iter != panel_windows_.end(); ++iter) {
    aura::Window* panel = iter->window;
    views::Widget* callout_widget = iter->callout_widget;

    gfx::Rect bounds = panel->GetBoundsInRootWindow();
    gfx::Rect icon_bounds =
        launcher_->GetScreenBoundsOfItemIconForWindow(panel);
    if (icon_bounds.IsEmpty() || !panel->IsVisible() ||
        panel == dragged_panel_) {
      callout_widget->Hide();
      continue;
    }

    gfx::Rect callout_bounds = callout_widget->GetWindowBoundsInScreen();
    if (horizontal) {
      callout_bounds.set_x(
          icon_bounds.x() + (icon_bounds.width() - callout_bounds.width()) / 2);
    } else {
      callout_bounds.set_y(
          icon_bounds.y() + (icon_bounds.height() -
                             callout_bounds.height()) / 2);
    }
    switch (alignment) {
      case SHELF_ALIGNMENT_BOTTOM:
        callout_bounds.set_y(bounds.bottom());
        break;
      case SHELF_ALIGNMENT_LEFT:
        callout_bounds.set_x(bounds.x() - callout_bounds.width());
        break;
      case SHELF_ALIGNMENT_RIGHT:
        callout_bounds.set_x(bounds.right());
        break;
      case SHELF_ALIGNMENT_TOP:
        callout_bounds.set_y(bounds.y() - callout_bounds.height());
        break;
    }
    callout_bounds = ScreenAsh::ConvertRectFromScreen(
        callout_widget->GetNativeWindow()->parent(),
        callout_bounds);

    SetChildBoundsDirect(callout_widget->GetNativeWindow(), callout_bounds);
    panel_container_->StackChildAbove(callout_widget->GetNativeWindow(),
                                      panel);
    callout_widget->Show();
  }
}

}  // namespace internal
}  // namespace ash
