// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/overview/window_grid.h"

#include <algorithm>
#include <functional>
#include <set>
#include <utility>
#include <vector>

#include "ash/common/ash_switches.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/overview/scoped_overview_animation_settings.h"
#include "ash/common/wm/overview/scoped_overview_animation_settings_factory.h"
#include "ash/common/wm/overview/scoped_transform_overview_window.h"
#include "ash/common/wm/overview/window_selector.h"
#include "ash/common/wm/overview/window_selector_item.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm/wm_screen_util.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_window.h"
#include "base/command_line.h"
#include "base/i18n/string_search.h"
#include "base/memory/scoped_vector.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace ash {
namespace {

using Windows = std::vector<WmWindow*>;

// An observer which holds onto the passed widget until the animation is
// complete.
class CleanupWidgetAfterAnimationObserver
    : public ui::ImplicitAnimationObserver {
 public:
  explicit CleanupWidgetAfterAnimationObserver(
      std::unique_ptr<views::Widget> widget);
  ~CleanupWidgetAfterAnimationObserver() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

 private:
  std::unique_ptr<views::Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(CleanupWidgetAfterAnimationObserver);
};

CleanupWidgetAfterAnimationObserver::CleanupWidgetAfterAnimationObserver(
    std::unique_ptr<views::Widget> widget)
    : widget_(std::move(widget)) {}

CleanupWidgetAfterAnimationObserver::~CleanupWidgetAfterAnimationObserver() {}

void CleanupWidgetAfterAnimationObserver::OnImplicitAnimationsCompleted() {
  delete this;
}

// A comparator for locating a given target window.
struct WindowSelectorItemComparator {
  explicit WindowSelectorItemComparator(const WmWindow* target_window)
      : target(target_window) {}

  bool operator()(WindowSelectorItem* window) const {
    return window->GetWindow() == target;
  }

  const WmWindow* target;
};

// Conceptually the window overview is a table or grid of cells having this
// fixed aspect ratio. The number of columns is determined by maximizing the
// area of them based on the number of window_list.
const float kCardAspectRatio = 4.0f / 3.0f;

// The minimum number of cards along the major axis (i.e. horizontally on a
// landscape orientation).
const int kMinCardsMajor = 3;

const int kOverviewSelectorTransitionMilliseconds = 250;

// The color and opacity of the screen shield in overview.
const SkColor kShieldColor = SkColorSetARGB(178, 0, 0, 0);

// The color and opacity of the overview selector.
const SkColor kWindowSelectionColor = SkColorSetARGB(128, 0, 0, 0);
const SkColor kWindowSelectionColorMD = SkColorSetARGB(51, 255, 255, 255);
const SkColor kWindowSelectionBorderColor = SkColorSetARGB(38, 255, 255, 255);
const SkColor kWindowSelectionBorderColorMD = SkColorSetARGB(76, 255, 255, 255);

// Border thickness of overview selector.
const int kWindowSelectionBorderThickness = 2;
const int kWindowSelectionBorderThicknessMD = 1;

// Corner radius of the overview selector border.
const int kWindowSelectionRadius = 0;
const int kWindowSelectionRadiusMD = 4;

// The minimum amount of spacing between the bottom of the text filtering
// text field and the top of the selection widget on the first row of items.
const int kTextFilterBottomMargin = 5;

// In the conceptual overview table, the window margin is the space reserved
// around the window within the cell. This margin does not overlap so the
// closest distance between adjacent windows will be twice this amount.
const int kWindowMarginMD = 5;

// Windows are not allowed to get taller than this.
const int kMaxHeight = 512;

// Margins reserved in the overview mode.
const float kOverviewInsetRatio = 0.05f;

// Additional vertical inset reserved for windows in overview mode.
const float kOverviewVerticalInset = 0.1f;

// A View having rounded corners and a specified background color which is
// only painted within the bounds defined by the rounded corners.
// TODO(varkha): This duplicates code from RoundedImageView. Refactor these
//               classes and move into ui/views.
class RoundedRectView : public views::View {
 public:
  RoundedRectView(int corner_radius, SkColor background)
      : corner_radius_(corner_radius), background_(background) {}

  ~RoundedRectView() override {}

  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    SkScalar radius = SkIntToScalar(corner_radius_);
    const SkScalar kRadius[8] = {radius, radius, radius, radius,
                                 radius, radius, radius, radius};
    SkPath path;
    gfx::Rect bounds(size());
    bounds.set_height(bounds.height() + radius);
    path.addRoundRect(gfx::RectToSkRect(bounds), kRadius);

    SkPaint paint;
    paint.setAntiAlias(true);
    canvas->ClipPath(path, true);
    canvas->DrawColor(background_);
  }

 private:
  int corner_radius_;
  SkColor background_;

  DISALLOW_COPY_AND_ASSIGN(RoundedRectView);
};

// BackgroundWith1PxBorder renders a solid background color, with a one pixel
// border with rounded corners. This accounts for the scaling of the canvas, so
// that the border is 1 pixel thick regardless of display scaling.
class BackgroundWith1PxBorder : public views::Background {
 public:
  BackgroundWith1PxBorder(SkColor background,
                          SkColor border_color,
                          int border_thickness,
                          int corner_radius);

  void Paint(gfx::Canvas* canvas, views::View* view) const override;

 private:
  // Color for the one pixel border.
  SkColor border_color_;

  // Thickness of border inset.
  int border_thickness_;

  // Corner radius of the inside edge of the roundrect border stroke.
  int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundWith1PxBorder);
};

BackgroundWith1PxBorder::BackgroundWith1PxBorder(SkColor background,
                                                 SkColor border_color,
                                                 int border_thickness,
                                                 int corner_radius)
    : border_color_(border_color),
      border_thickness_(border_thickness),
      corner_radius_(corner_radius) {
  SetNativeControlColor(background);
}

void BackgroundWith1PxBorder::Paint(gfx::Canvas* canvas,
                                    views::View* view) const {
  gfx::RectF border_rect_f(view->GetContentsBounds());

  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();
  border_rect_f.Scale(scale);
  const float inset = border_thickness_ * scale - 0.5f;
  border_rect_f.Inset(inset, inset);

  SkPath path;
  const SkScalar scaled_corner_radius =
      SkFloatToScalar(corner_radius_ * scale + 0.5f);
  path.addRoundRect(gfx::RectFToSkRect(border_rect_f), scaled_corner_radius,
                    scaled_corner_radius);

  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(1);
  paint.setAntiAlias(true);

  SkPath stroke_path;
  paint.getFillPath(path, &stroke_path);

  SkPath fill_path;
  Op(path, stroke_path, kDifference_SkPathOp, &fill_path);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(get_color());
  canvas->sk_canvas()->drawPath(fill_path, paint);

  if (border_thickness_ > 0) {
    paint.setColor(border_color_);
    canvas->sk_canvas()->drawPath(stroke_path, paint);
  }
}

// Returns the vector for the fade in animation.
gfx::Vector2d GetSlideVectorForFadeIn(WindowSelector::Direction direction,
                                      const gfx::Rect& bounds) {
  gfx::Vector2d vector;
  const bool material = ash::MaterialDesignController::IsOverviewMaterial();
  switch (direction) {
    case WindowSelector::UP:
      if (!material) {
        vector.set_y(-bounds.height());
        break;
      }
    case WindowSelector::LEFT:
      vector.set_x(-bounds.width());
      break;
    case WindowSelector::DOWN:
      if (!material) {
        vector.set_y(bounds.height());
        break;
      }
    case WindowSelector::RIGHT:
      vector.set_x(bounds.width());
      break;
  }
  return vector;
}

// Given |root_window|, calculates the item size necessary to fit |items|
// items in the window selection. |bounding_rect| is set to the centered
// rectangle containing the grid and |item_size| is set to the size of each
// individual item.
void CalculateOverviewSizes(WmWindow* root_window,
                            size_t items,
                            int text_filter_bottom,
                            gfx::Rect* bounding_rect,
                            gfx::Size* item_size) {
  gfx::Rect total_bounds = root_window->ConvertRectToScreen(
      wm::GetDisplayWorkAreaBoundsInParent(root_window->GetChildByShellWindowId(
          kShellWindowId_DefaultContainer)));

  // Reserve space at the top for the text filtering textbox to appear.
  total_bounds.Inset(0, text_filter_bottom + kTextFilterBottomMargin, 0, 0);

  // Find the minimum number of windows per row that will fit all of the
  // windows on screen.
  int num_columns = std::max(
      total_bounds.width() > total_bounds.height() ? kMinCardsMajor : 1,
      static_cast<int>(ceil(sqrt(total_bounds.width() * items /
                                 (kCardAspectRatio * total_bounds.height())))));
  int num_rows = ((items + num_columns - 1) / num_columns);
  item_size->set_width(std::min(
      static_cast<int>(total_bounds.width() / num_columns),
      static_cast<int>(total_bounds.height() * kCardAspectRatio / num_rows)));
  item_size->set_height(
      static_cast<int>(item_size->width() / kCardAspectRatio));
  item_size->SetToMax(gfx::Size(1, 1));

  bounding_rect->set_width(std::min(static_cast<int>(items), num_columns) *
                           item_size->width());
  bounding_rect->set_height(num_rows * item_size->height());
  // Calculate the X and Y offsets necessary to center the grid.
  bounding_rect->set_x(total_bounds.x() +
                       (total_bounds.width() - bounding_rect->width()) / 2);
  bounding_rect->set_y(total_bounds.y() +
                       (total_bounds.height() - bounding_rect->height()) / 2);
}

// Reorders the list of windows |items| in |root_window| in an attempt to
// minimize the distance each window will travel to enter overview. For
// equidistant windows preserves a stable order between overview sessions
// by comparing window pointers.
void ReorderItemsGreedyLeastMovement(std::vector<WmWindow*>* items,
                                     WmWindow* root_window,
                                     int text_filter_bottom) {
  if (items->empty())
    return;
  gfx::Rect bounding_rect;
  gfx::Size item_size;
  CalculateOverviewSizes(root_window, items->size(), text_filter_bottom,
                         &bounding_rect, &item_size);
  int num_columns = std::min(static_cast<int>(items->size()),
                             bounding_rect.width() / item_size.width());
  for (size_t i = 0; i < items->size(); ++i) {
    int column = i % num_columns;
    int row = i / num_columns;
    gfx::Point overview_item_center(
        bounding_rect.x() + column * item_size.width() + item_size.width() / 2,
        bounding_rect.y() + row * item_size.height() + item_size.height() / 2);
    // Find the nearest window for this position.
    size_t swap_index = i;
    int64_t shortest_distance = std::numeric_limits<int64_t>::max();
    for (size_t j = i; j < items->size(); ++j) {
      WmWindow* window = (*items)[j];
      const gfx::Rect screen_target_bounds =
          window->ConvertRectToScreen(window->GetTargetBounds());
      int64_t distance =
          (screen_target_bounds.CenterPoint() - overview_item_center)
              .LengthSquared();
      // We compare raw pointers to create a stable ordering given two windows
      // with the same center point.
      if (distance < shortest_distance ||
          (distance == shortest_distance && window < (*items)[swap_index])) {
        shortest_distance = distance;
        swap_index = j;
      }
    }
    if (swap_index > i)
      std::swap((*items)[i], (*items)[swap_index]);
  }
}

// Creates and returns a background translucent widget parented in
// |root_window|'s default container and having |background_color|.
// When |border_thickness| is non-zero, a border is created having
// |border_color|, otherwise |border_color| parameter is ignored.
views::Widget* CreateBackgroundWidget(WmWindow* root_window,
                                      SkColor background_color,
                                      int border_thickness,
                                      int border_radius,
                                      SkColor border_color) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.keep_on_top = false;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = false;
  widget->set_focus_on_creation(false);
  // Parenting in kShellWindowId_DesktopBackgroundContainer allows proper
  // layering of the shield and selection widgets. Since that container is
  // created with USE_LOCAL_COORDINATES BoundsInScreenBehavior local bounds in
  // |root_window_| need to be provided.
  root_window->GetRootWindowController()->ConfigureWidgetInitParamsForContainer(
      widget, kShellWindowId_DesktopBackgroundContainer, &params);
  widget->Init(params);
  WmWindow* widget_window = WmLookup::Get()->GetWindowForWidget(widget);
  // Disable the "bounce in" animation when showing the window.
  widget_window->SetVisibilityAnimationTransition(::wm::ANIMATE_NONE);
  // The background widget should not activate the shelf when passing under it.
  widget_window->GetWindowState()->set_ignored_by_shelf(true);

  views::View* content_view =
      new RoundedRectView(border_radius, SK_ColorTRANSPARENT);
  if (ash::MaterialDesignController::IsOverviewMaterial()) {
    content_view->set_background(new BackgroundWith1PxBorder(
        background_color, border_color, border_thickness, border_radius));
  } else {
    content_view->set_background(
        views::Background::CreateSolidBackground(background_color));
    if (border_thickness) {
      content_view->SetBorder(
          views::Border::CreateSolidBorder(border_thickness, border_color));
    }
  }
  widget->SetContentsView(content_view);
  widget_window->GetParent()->StackChildAtTop(widget_window);
  widget->Show();
  // New background widget starts with 0 opacity and then fades in.
  widget_window->SetOpacity(0.f);
  return widget;
}

}  // namespace

WindowGrid::WindowGrid(WmWindow* root_window,
                       const std::vector<WmWindow*>& windows,
                       WindowSelector* window_selector)
    : root_window_(root_window),
      window_selector_(window_selector),
      selected_index_(0),
      num_columns_(0) {
  std::vector<WmWindow*> windows_in_root;
  for (auto* window : windows) {
    if (window->GetRootWindow() == root_window)
      windows_in_root.push_back(window);
  }

  if (!ash::MaterialDesignController::IsOverviewMaterial() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableStableOverviewOrder)) {
    // Reorder windows to try to minimize movement to target overview positions.
    // This also creates a stable window ordering.
    ReorderItemsGreedyLeastMovement(&windows_in_root, root_window_,
                                    window_selector_->text_filter_bottom());
  }
  for (auto* window : windows_in_root) {
    window->AddObserver(this);
    observed_windows_.insert(window);
    window_list_.push_back(new WindowSelectorItem(window, window_selector_));
  }
}

WindowGrid::~WindowGrid() {
  for (WmWindow* window : observed_windows_)
    window->RemoveObserver(this);
}

void WindowGrid::PrepareForOverview() {
  if (ash::MaterialDesignController::IsOverviewMaterial())
    InitShieldWidget();
  for (auto iter = window_list_.begin(); iter != window_list_.end(); ++iter)
    (*iter)->PrepareForOverview();
}

void WindowGrid::PositionWindowsMD(bool animate) {
  if (window_list_.empty())
    return;
  gfx::Rect total_bounds =
      root_window_->ConvertRectToScreen(wm::GetDisplayWorkAreaBoundsInParent(
          root_window_->GetChildByShellWindowId(
              kShellWindowId_DefaultContainer)));
  // Windows occupy vertically centered area with additional vertical insets.
  int horizontal_inset =
      gfx::ToFlooredInt(std::min(kOverviewInsetRatio * total_bounds.width(),
                                 kOverviewInsetRatio * total_bounds.height()));
  int vertical_inset =
      horizontal_inset +
      kOverviewVerticalInset * (total_bounds.height() - 2 * horizontal_inset);
  total_bounds.Inset(std::max(0, horizontal_inset - kWindowMarginMD),
                     std::max(0, vertical_inset - kWindowMarginMD));
  std::vector<gfx::Rect> rects;

  // Keep track of the lowest coordinate.
  int max_bottom = total_bounds.y();

  // Right bound of the narrowest row.
  int min_right = total_bounds.right();
  // Right bound of the widest row.
  int max_right = total_bounds.x();

  // Keep track of the difference between the narrowest and the widest row.
  // Initially this is set to the worst it can ever be assuming the windows fit.
  int width_diff = total_bounds.width();

  // Initially allow the windows to occupy all available width. Shrink this
  // available space horizontally to find the breakdown into rows that achieves
  // the minimal |width_diff|.
  int right_bound = total_bounds.right();

  // Determine the optimal height bisecting between |low_height| and
  // |high_height|. Once this optimal height is known, |height_fixed| is set to
  // true and the rows are balanced by repeatedly squeezing the widest row to
  // cause windows to overflow to the subsequent rows.
  int low_height = 2 * kWindowMarginMD;
  int high_height =
      std::max(low_height, static_cast<int>(total_bounds.height() + 1));
  int height = 0.5 * (low_height + high_height);
  bool height_fixed = false;

  // Repeatedly try to fit the windows |rects| within |right_bound|.
  // If a maximum |height| is found such that all window |rects| fit, this
  // fitting continues while shrinking the |right_bound| in order to balance the
  // rows. If the windows fit the |right_bound| would have been decremented at
  // least once so it needs to be incremented once before getting out of this
  // loop and one additional pass made to actually fit the |rects|.
  // If the |rects| cannot fit (e.g. there are too many windows) the bisection
  // will still finish and we might increment the |right_bound| once pixel extra
  // which is acceptable since there is an unused margin on the right.
  bool make_last_adjustment = false;
  while (true) {
    gfx::Rect overview_bounds(total_bounds);
    overview_bounds.set_width(right_bound - total_bounds.x());
    bool windows_fit = FitWindowRectsInBounds(
        overview_bounds, std::min(kMaxHeight + 2 * kWindowMarginMD, height),
        &rects, &max_bottom, &min_right, &max_right);

    if (height_fixed) {
      if (!windows_fit) {
        // Revert the previous change to |right_bound| and do one last pass.
        right_bound++;
        make_last_adjustment = true;
        break;
      }
      // Break if all the windows are zero-width at the current scale.
      if (max_right <= total_bounds.x())
        break;
    } else {
      // Find the optimal row height bisecting between |low_height| and
      // |high_height|.
      if (windows_fit)
        low_height = height;
      else
        high_height = height;
      height = 0.5 * (low_height + high_height);
      // When height can no longer be improved, start balancing the rows.
      if (height == low_height)
        height_fixed = true;
    }

    if (windows_fit && height_fixed) {
      if (max_right - min_right <= width_diff) {
        // Row alignment is getting better. Try to shrink the |right_bound| in
        // order to squeeze the widest row.
        right_bound = max_right - 1;
        width_diff = max_right - min_right;
      } else {
        // Row alignment is getting worse.
        // Revert the previous change to |right_bound| and do one last pass.
        right_bound++;
        make_last_adjustment = true;
        break;
      }
    }
  }
  // Once the windows in |window_list_| no longer fit, the change to
  // |right_bound| was reverted. Perform one last pass to position the |rects|.
  if (make_last_adjustment) {
    gfx::Rect overview_bounds(total_bounds);
    overview_bounds.set_width(right_bound - total_bounds.x());
    FitWindowRectsInBounds(overview_bounds,
                           std::min(kMaxHeight + 2 * kWindowMarginMD, height),
                           &rects, &max_bottom, &min_right, &max_right);
  }
  // Position the windows centering the left-aligned rows vertically.
  gfx::Vector2d offset(0, (total_bounds.bottom() - max_bottom) / 2);
  for (size_t i = 0; i < window_list_.size(); ++i) {
    window_list_[i]->SetBounds(
        rects[i] + offset,
        animate
            ? OverviewAnimationType::OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS
            : OverviewAnimationType::OVERVIEW_ANIMATION_NONE);
  }

  // If the selection widget is active, reposition it without any animation.
  if (selection_widget_)
    MoveSelectionWidgetToTarget(animate);
}

void WindowGrid::PositionWindows(bool animate) {
  if (ash::MaterialDesignController::IsOverviewMaterial()) {
    DCHECK(shield_widget_.get());
    // Keep the background shield widget covering the whole screen.
    WmWindow* widget_window =
        WmLookup::Get()->GetWindowForWidget(shield_widget_.get());
    const gfx::Rect bounds = widget_window->GetParent()->GetBounds();
    widget_window->SetBounds(bounds);
    PositionWindowsMD(animate);
    return;
  }
  CHECK(!window_list_.empty());
  gfx::Rect bounding_rect;
  gfx::Size item_size;
  CalculateOverviewSizes(root_window_, window_list_.size(),
                         window_selector_->text_filter_bottom(), &bounding_rect,
                         &item_size);
  num_columns_ = std::min(static_cast<int>(window_list_.size()),
                          bounding_rect.width() / item_size.width());
  for (size_t i = 0; i < window_list_.size(); ++i) {
    gfx::Transform transform;
    int column = i % num_columns_;
    int row = i / num_columns_;
    gfx::Rect target_bounds(item_size.width() * column + bounding_rect.x(),
                            item_size.height() * row + bounding_rect.y(),
                            item_size.width(), item_size.height());
    window_list_[i]->SetBounds(
        target_bounds,
        animate
            ? OverviewAnimationType::OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS
            : OverviewAnimationType::OVERVIEW_ANIMATION_NONE);
  }

  // If the selection widget is active, reposition it without any animation.
  if (selection_widget_)
    MoveSelectionWidgetToTarget(animate);
}

bool WindowGrid::Move(WindowSelector::Direction direction, bool animate) {
  bool recreate_selection_widget = false;
  bool out_of_bounds = false;
  bool changed_selection_index = false;
  const bool material = ash::MaterialDesignController::IsOverviewMaterial();
  gfx::Rect old_bounds;
  if (SelectedWindow()) {
    old_bounds = SelectedWindow()->target_bounds();
    // Make the old selected window header non-transparent first.
    SelectedWindow()->SetSelected(false);
  }

  // With Material Design enabled [up] key is equivalent to [left] key and
  // [down] key is equivalent to [right] key.
  if (!selection_widget_) {
    switch (direction) {
      case WindowSelector::UP:
        if (!material) {
          selected_index_ =
              (window_list_.size() / num_columns_) * num_columns_ - 1;
          break;
        }
      case WindowSelector::LEFT:
        selected_index_ = window_list_.size() - 1;
        break;
      case WindowSelector::DOWN:
      case WindowSelector::RIGHT:
        selected_index_ = 0;
        break;
    }
    changed_selection_index = true;
  }
  while (!changed_selection_index ||
         (!out_of_bounds && window_list_[selected_index_]->dimmed())) {
    switch (direction) {
      case WindowSelector::UP:
        if (!material) {
          if (selected_index_ == 0)
            out_of_bounds = true;
          if (selected_index_ < num_columns_) {
            selected_index_ +=
                num_columns_ *
                    ((window_list_.size() - selected_index_) / num_columns_) -
                1;
            recreate_selection_widget = true;
          } else {
            selected_index_ -= num_columns_;
          }
          break;
        }
      case WindowSelector::LEFT:
        if (selected_index_ == 0)
          out_of_bounds = true;
        selected_index_--;
        if (!material && (selected_index_ + 1) % num_columns_ == 0)
          recreate_selection_widget = true;
        break;
      case WindowSelector::DOWN:
        if (!material) {
          selected_index_ += num_columns_;
          if (selected_index_ >= window_list_.size()) {
            selected_index_ = (selected_index_ + 1) % num_columns_;
            if (selected_index_ == 0)
              out_of_bounds = true;
            recreate_selection_widget = true;
          }
          break;
        }
      case WindowSelector::RIGHT:
        if (selected_index_ >= window_list_.size() - 1)
          out_of_bounds = true;
        selected_index_++;
        if (!material && selected_index_ % num_columns_ == 0)
          recreate_selection_widget = true;
        break;
    }
    if (material) {
      if (!out_of_bounds && SelectedWindow()) {
        if (SelectedWindow()->target_bounds().y() != old_bounds.y())
          recreate_selection_widget = true;
      }
    }
    changed_selection_index = true;
  }

  MoveSelectionWidget(direction, recreate_selection_widget, out_of_bounds,
                      animate);

  // Make the new selected window header fully transparent.
  if (SelectedWindow())
    SelectedWindow()->SetSelected(true);
  return out_of_bounds;
}

WindowSelectorItem* WindowGrid::SelectedWindow() const {
  if (!selection_widget_)
    return nullptr;
  CHECK(selected_index_ < window_list_.size());
  return window_list_[selected_index_];
}

bool WindowGrid::Contains(const WmWindow* window) const {
  for (const WindowSelectorItem* window_item : window_list_) {
    if (window_item->Contains(window))
      return true;
  }
  return false;
}

void WindowGrid::FilterItems(const base::string16& pattern) {
  base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents finder(pattern);
  for (auto iter = window_list_.begin(); iter != window_list_.end(); iter++) {
    if (finder.Search((*iter)->GetWindow()->GetTitle(), nullptr, nullptr)) {
      (*iter)->SetDimmed(false);
    } else {
      (*iter)->SetDimmed(true);
      if (selection_widget_ && SelectedWindow() == *iter) {
        SelectedWindow()->SetSelected(false);
        selection_widget_.reset();
      }
    }
  }
}

void WindowGrid::WindowClosing(WindowSelectorItem* window) {
  if (!selection_widget_ || SelectedWindow() != window)
    return;
  WmWindow* selection_widget_window =
      WmLookup::Get()->GetWindowForWidget(selection_widget_.get());
  std::unique_ptr<ScopedOverviewAnimationSettings> animation_settings_label =
      ScopedOverviewAnimationSettingsFactory::Get()
          ->CreateOverviewAnimationSettings(
              OverviewAnimationType::OVERVIEW_ANIMATION_CLOSING_SELECTOR_ITEM,
              selection_widget_window);
  selection_widget_->SetOpacity(0.f);
}

void WindowGrid::OnWindowDestroying(WmWindow* window) {
  window->RemoveObserver(this);
  observed_windows_.erase(window);
  ScopedVector<WindowSelectorItem>::iterator iter =
      std::find_if(window_list_.begin(), window_list_.end(),
                   WindowSelectorItemComparator(window));

  DCHECK(iter != window_list_.end());

  size_t removed_index = iter - window_list_.begin();
  window_list_.erase(iter);

  if (empty()) {
    // If the grid is now empty, notify the window selector so that it erases us
    // from its grid list.
    window_selector_->OnGridEmpty(this);
    return;
  }

  // If selecting, update the selection index.
  if (selection_widget_) {
    bool send_focus_alert = selected_index_ == removed_index;
    if (selected_index_ >= removed_index && selected_index_ != 0)
      selected_index_--;
    SelectedWindow()->SetSelected(true);
    if (send_focus_alert)
      SelectedWindow()->SendAccessibleSelectionEvent();
  }

  PositionWindows(true);
}

void WindowGrid::OnWindowBoundsChanged(WmWindow* window,
                                       const gfx::Rect& old_bounds,
                                       const gfx::Rect& new_bounds) {
  auto iter = std::find_if(window_list_.begin(), window_list_.end(),
                           WindowSelectorItemComparator(window));
  DCHECK(iter != window_list_.end());

  // Immediately finish any active bounds animation.
  window->StopAnimatingProperty(ui::LayerAnimationElement::BOUNDS);

  if (ash::MaterialDesignController::IsOverviewMaterial()) {
    PositionWindows(false);
    return;
  }
  // Recompute the transform for the window.
  (*iter)->RecomputeWindowTransforms();
}

void WindowGrid::InitShieldWidget() {
  shield_widget_.reset(CreateBackgroundWidget(root_window_, kShieldColor, 0, 0,
                                              SK_ColorTRANSPARENT));

  WmWindow* widget_window =
      WmLookup::Get()->GetWindowForWidget(shield_widget_.get());
  const gfx::Rect bounds = widget_window->GetParent()->GetBounds();
  widget_window->SetBounds(bounds);

  ui::ScopedLayerAnimationSettings animation_settings(
      widget_window->GetLayer()->GetAnimator());
  animation_settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      kOverviewSelectorTransitionMilliseconds));
  animation_settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
  animation_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  shield_widget_->SetOpacity(1.f);
}

void WindowGrid::InitSelectionWidget(WindowSelector::Direction direction) {
  const bool material = ash::MaterialDesignController::IsOverviewMaterial();
  const int border_thickness = material ? kWindowSelectionBorderThicknessMD
                                        : kWindowSelectionBorderThickness;
  const int border_color =
      material ? kWindowSelectionBorderColorMD : kWindowSelectionBorderColor;
  const int selection_color =
      material ? kWindowSelectionColorMD : kWindowSelectionColor;
  const int border_radius =
      material ? kWindowSelectionRadiusMD : kWindowSelectionRadius;
  selection_widget_.reset(CreateBackgroundWidget(root_window_, selection_color,
                                                 border_thickness,
                                                 border_radius, border_color));

  WmWindow* widget_window =
      WmLookup::Get()->GetWindowForWidget(selection_widget_.get());
  const gfx::Rect target_bounds =
      root_window_->ConvertRectFromScreen(SelectedWindow()->target_bounds());
  gfx::Vector2d fade_out_direction =
      GetSlideVectorForFadeIn(direction, target_bounds);
  widget_window->SetBounds(target_bounds - fade_out_direction);
}

void WindowGrid::MoveSelectionWidget(WindowSelector::Direction direction,
                                     bool recreate_selection_widget,
                                     bool out_of_bounds,
                                     bool animate) {
  // If the selection widget is already active, fade it out in the selection
  // direction.
  if (selection_widget_ && (recreate_selection_widget || out_of_bounds)) {
    // Animate the old selection widget and then destroy it.
    views::Widget* old_selection = selection_widget_.get();
    WmWindow* old_selection_window =
        WmLookup::Get()->GetWindowForWidget(old_selection);
    gfx::Vector2d fade_out_direction =
        GetSlideVectorForFadeIn(direction, old_selection_window->GetBounds());

    ui::ScopedLayerAnimationSettings animation_settings(
        old_selection_window->GetLayer()->GetAnimator());
    animation_settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        kOverviewSelectorTransitionMilliseconds));
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    animation_settings.SetTweenType(gfx::Tween::FAST_OUT_LINEAR_IN);
    // CleanupWidgetAfterAnimationObserver will delete itself (and the
    // widget) when the movement animation is complete.
    animation_settings.AddObserver(
        new CleanupWidgetAfterAnimationObserver(std::move(selection_widget_)));
    old_selection->SetOpacity(0.f);
    old_selection_window->SetBounds(old_selection_window->GetBounds() +
                                    fade_out_direction);
    old_selection->Hide();
  }
  if (out_of_bounds)
    return;

  if (!selection_widget_)
    InitSelectionWidget(direction);
  // Send an a11y alert so that if ChromeVox is enabled, the item label is
  // read.
  SelectedWindow()->SendAccessibleSelectionEvent();
  // The selection widget is moved to the newly selected item in the same
  // grid.
  MoveSelectionWidgetToTarget(animate);
}

void WindowGrid::MoveSelectionWidgetToTarget(bool animate) {
  gfx::Rect bounds =
      root_window_->ConvertRectFromScreen(SelectedWindow()->target_bounds());
  if (animate) {
    WmWindow* selection_widget_window =
        WmLookup::Get()->GetWindowForWidget(selection_widget_.get());
    ui::ScopedLayerAnimationSettings animation_settings(
        selection_widget_window->GetLayer()->GetAnimator());
    animation_settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        kOverviewSelectorTransitionMilliseconds));
    animation_settings.SetTweenType(
        ash::MaterialDesignController::IsOverviewMaterial()
            ? gfx::Tween::EASE_IN_OUT
            : gfx::Tween::LINEAR_OUT_SLOW_IN);
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    selection_widget_->SetBounds(bounds);
    selection_widget_->SetOpacity(1.f);
    return;
  }
  selection_widget_->SetBounds(bounds);
  selection_widget_->SetOpacity(1.f);
}

bool WindowGrid::FitWindowRectsInBounds(const gfx::Rect& bounds,
                                        int height,
                                        std::vector<gfx::Rect>* rects,
                                        int* max_bottom,
                                        int* min_right,
                                        int* max_right) {
  rects->resize(window_list_.size());
  bool windows_fit = true;

  // Start in the top-left corner of |bounds|.
  int left = bounds.x();
  int top = bounds.y();

  // Keep track of the lowest coordinate.
  *max_bottom = bounds.y();

  // Right bound of the narrowest row.
  *min_right = bounds.right();
  // Right bound of the widest row.
  *max_right = bounds.x();

  // With Material Design all elements are of same height and only the height is
  // necessary to determine each item's scale.
  const gfx::Size item_size(0, height);
  size_t i = 0;
  for (auto* window : window_list_) {
    const gfx::Rect target_bounds = window->GetWindow()->GetTargetBounds();
    const int width =
        std::max(1, gfx::ToFlooredInt(target_bounds.width() *
                                      window->GetItemScale(item_size)) +
                        2 * kWindowMarginMD);
    if (left + width > bounds.right()) {
      // Move to the next row if possible.
      if (*min_right > left)
        *min_right = left;
      if (*max_right < left)
        *max_right = left;
      top += height;

      // Check if the new row reaches the bottom or if the first item in the new
      // row does not fit within the available width.
      if (top + height > bounds.bottom() ||
          bounds.x() + width > bounds.right()) {
        windows_fit = false;
        break;
      }
      left = bounds.x();
    }

    // Position the current rect.
    (*rects)[i].SetRect(left, top, width, height);

    // Increment horizontal position using sanitized positive |width()|.
    left += (*rects)[i].width();

    if (++i == window_list_.size()) {
      // Update the narrowest and widest row width for the last row.
      if (*min_right > left)
        *min_right = left;
      if (*max_right < left)
        *max_right = left;
    }
    *max_bottom = top + height;
  }
  return windows_fit;
}

}  // namespace ash
