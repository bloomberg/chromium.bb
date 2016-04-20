// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/dock/docked_window_layout_manager.h"

#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/common/window_animation_types.h"
#include "ash/wm/common/window_parenting_utils.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "grit/ash_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

// Minimum, maximum width of the dock area and a width of the gap
// static
const int DockedWindowLayoutManager::kMaxDockWidth = 360;
// static
const int DockedWindowLayoutManager::kMinDockWidth = 200;
// static
const int DockedWindowLayoutManager::kMinDockGap = 2;
// static
const int DockedWindowLayoutManager::kIdealWidth = 250;
const int kMinimumHeight = 250;
const int kSlideDurationMs = 120;
const int kFadeDurationMs = 60;
const int kMinimizeDurationMs = 720;

class DockedBackgroundWidget : public views::Widget,
                               public BackgroundAnimatorDelegate,
                               public ShelfLayoutManagerObserver {
 public:
  explicit DockedBackgroundWidget(DockedWindowLayoutManager* manager)
      : manager_(manager),
        alignment_(DOCKED_ALIGNMENT_NONE),
        background_animator_(this, 0, kShelfBackgroundAlpha),
        alpha_(0),
        opaque_background_(ui::LAYER_SOLID_COLOR),
        visible_background_type_(
            manager_->shelf()->shelf_widget()->GetBackgroundType()),
        visible_background_change_type_(BACKGROUND_CHANGE_IMMEDIATE) {
    manager_->shelf()->shelf_layout_manager()->AddObserver(this);
    InitWidget(manager_->dock_container());
  }

  ~DockedBackgroundWidget() override {
    manager_->shelf()->shelf_layout_manager()->RemoveObserver(this);
  }

  // Sets widget bounds and sizes opaque background layer to fill the widget.
  void SetBackgroundBounds(const gfx::Rect bounds, DockedAlignment alignment) {
    SetBounds(bounds);
    opaque_background_.SetBounds(gfx::Rect(bounds.size()));
    alignment_ = alignment;
  }

 private:
  // views::Widget:
  void OnNativeWidgetVisibilityChanged(bool visible) override {
    views::Widget::OnNativeWidgetVisibilityChanged(visible);
    UpdateBackground();
  }

  void OnNativeWidgetPaint(const ui::PaintContext& context) override {
    gfx::Rect local_window_bounds(GetWindowBoundsInScreen().size());
    ui::PaintRecorder recorder(context, local_window_bounds.size());
    const gfx::ImageSkia& shelf_background(
        alignment_ == DOCKED_ALIGNMENT_LEFT ?
            shelf_background_left_ : shelf_background_right_);
    SkPaint paint;
    paint.setAlpha(alpha_);
    recorder.canvas()->DrawImageInt(
        shelf_background, 0, 0, shelf_background.width(),
        shelf_background.height(),
        alignment_ == DOCKED_ALIGNMENT_LEFT
            ? local_window_bounds.width() - shelf_background.width()
            : 0,
        0, shelf_background.width(), local_window_bounds.height(), false,
        paint);
    recorder.canvas()->DrawImageInt(
        shelf_background,
        alignment_ == DOCKED_ALIGNMENT_LEFT ? 0 : shelf_background.width() - 1,
        0, 1, shelf_background.height(),
        alignment_ == DOCKED_ALIGNMENT_LEFT ? 0 : shelf_background.width(), 0,
        local_window_bounds.width() - shelf_background.width(),
        local_window_bounds.height(), false, paint);
  }

  // BackgroundAnimatorDelegate:
  void UpdateBackground(int alpha) override {
    alpha_ = alpha;
    SchedulePaintInRect(gfx::Rect(GetWindowBoundsInScreen().size()));
  }

  // ShelfLayoutManagerObserver:
  void OnBackgroundUpdated(ShelfBackgroundType background_type,
                           BackgroundAnimatorChangeType change_type) override {
    // Sets the background type. Starts an animation to transition to
    // |background_type| if the widget is visible. If the widget is not visible,
    // the animation is postponed till the widget becomes visible.
    visible_background_type_ = background_type;
    visible_background_change_type_ = change_type;
    if (IsVisible())
      UpdateBackground();
  }

  void InitWidget(aura::Window* parent) {
    views::Widget::InitParams params;
    params.type = views::Widget::InitParams::TYPE_POPUP;
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.keep_on_top = false;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.parent = parent;
    params.accept_events = false;
    set_focus_on_creation(false);
    Init(params);
    SetVisibilityChangedAnimationsEnabled(false);
    GetNativeWindow()->SetProperty(kStayInSameRootWindowKey, true);
    opaque_background_.SetColor(SK_ColorBLACK);
    opaque_background_.SetBounds(gfx::Rect(GetWindowBoundsInScreen().size()));
    opaque_background_.SetOpacity(0.0f);
    GetNativeWindow()->layer()->Add(&opaque_background_);

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    gfx::ImageSkia shelf_background =
        *rb.GetImageSkiaNamed(IDR_ASH_SHELF_BACKGROUND);
    shelf_background_left_ = gfx::ImageSkiaOperations::CreateRotatedImage(
        shelf_background, SkBitmapOperations::ROTATION_90_CW);
    shelf_background_right_ = gfx::ImageSkiaOperations::CreateRotatedImage(
        shelf_background, SkBitmapOperations::ROTATION_270_CW);

    // This background should be explicitly stacked below any windows already in
    // the dock, otherwise the z-order is set by the order in which windows were
    // added to the container, and UpdateStacking only manages user windows, not
    // the background widget.
    parent->StackChildAtBottom(GetNativeWindow());
  }

  // Transitions to |visible_background_type_| if the widget is visible and to
  // SHELF_BACKGROUND_DEFAULT if it is not.
  void UpdateBackground() {
    ShelfBackgroundType background_type = IsVisible() ?
        visible_background_type_ : SHELF_BACKGROUND_DEFAULT;
    BackgroundAnimatorChangeType change_type = IsVisible() ?
        visible_background_change_type_ : BACKGROUND_CHANGE_IMMEDIATE;

    float target_opacity =
        (background_type == SHELF_BACKGROUND_MAXIMIZED) ? 1.0f : 0.0f;
    std::unique_ptr<ui::ScopedLayerAnimationSettings>
        opaque_background_animation;
    if (change_type != BACKGROUND_CHANGE_IMMEDIATE) {
      opaque_background_animation.reset(new ui::ScopedLayerAnimationSettings(
          opaque_background_.GetAnimator()));
      opaque_background_animation->SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kTimeToSwitchBackgroundMs));
    }
    opaque_background_.SetOpacity(target_opacity);

    // TODO(varkha): use ui::Layer on both opaque_background and normal
    // background retire background_animator_ at all. It would be simpler.
    // See also ShelfWidget::SetPaintsBackground.
    background_animator_.SetPaintsBackground(
        background_type != SHELF_BACKGROUND_DEFAULT,
        change_type);
    SchedulePaintInRect(gfx::Rect(GetWindowBoundsInScreen().size()));
  }

  DockedWindowLayoutManager* manager_;

  DockedAlignment alignment_;

  // The animator for the background transitions.
  BackgroundAnimator background_animator_;

  // The alpha to use for drawing image assets covering the docked background.
  int alpha_;

  // Solid black background that can be made fully opaque.
  ui::Layer opaque_background_;

  // Backgrounds created from shelf background by 90 or 270 degree rotation.
  gfx::ImageSkia shelf_background_left_;
  gfx::ImageSkia shelf_background_right_;

  // The background type to use when the widget is visible. When not visible,
  // the widget uses SHELF_BACKGROUND_DEFAULT.
  ShelfBackgroundType visible_background_type_;

  // Whether the widget should animate to |visible_background_type_|.
  BackgroundAnimatorChangeType visible_background_change_type_;

  DISALLOW_COPY_AND_ASSIGN(DockedBackgroundWidget);
};

namespace {

// Returns true if a window is a popup or a transient child.
bool IsPopupOrTransient(const aura::Window* window) {
  return (window->type() == ui::wm::WINDOW_TYPE_POPUP ||
          ::wm::GetTransientParent(window));
}

// Certain windows (minimized, hidden or popups) are not docked and are ignored
// by layout logic even when they are children of a docked container.
bool IsWindowDocked(const aura::Window* window) {
  return (window->IsVisible() &&
          !wm::GetWindowState(window)->IsMinimized() &&
          !IsPopupOrTransient(window));
}

void UndockWindow(aura::Window* window) {
  gfx::Rect previous_bounds = window->bounds();
  aura::Window* old_parent = window->parent();
  aura::client::ParentWindowWithContext(window, window, gfx::Rect());
  if (window->parent() != old_parent) {
    wm::ReparentTransientChildrenOfChild(
        wm::WmWindowAura::Get(window), wm::WmWindowAura::Get(old_parent),
        wm::WmWindowAura::Get(window->parent()));
  }
  // Start maximize or fullscreen (affecting packaged apps) animation from
  // previous window bounds.
  window->layer()->SetBounds(previous_bounds);
}

// Returns width that is as close as possible to |target_width| while being
// consistent with docked min and max restrictions and respects the |window|'s
// minimum and maximum size.
int GetWindowWidthCloseTo(const aura::Window* window, int target_width) {
  if (!wm::GetWindowState(window)->CanResize()) {
    DCHECK_LE(window->bounds().width(),
              DockedWindowLayoutManager::kMaxDockWidth);
    return window->bounds().width();
  }
  int width = std::max(DockedWindowLayoutManager::kMinDockWidth,
                       std::min(target_width,
                                DockedWindowLayoutManager::kMaxDockWidth));
  if (window->delegate()) {
    if (window->delegate()->GetMinimumSize().width() != 0)
      width = std::max(width, window->delegate()->GetMinimumSize().width());
    if (window->delegate()->GetMaximumSize().width() != 0)
      width = std::min(width, window->delegate()->GetMaximumSize().width());
  }
  DCHECK_LE(width, DockedWindowLayoutManager::kMaxDockWidth);
  return width;
}

// Returns height that is as close as possible to |target_height| while
// respecting the |window|'s minimum and maximum size.
int GetWindowHeightCloseTo(const aura::Window* window, int target_height) {
  if (!wm::GetWindowState(window)->CanResize())
    return window->bounds().height();
  int minimum_height = kMinimumHeight;
  int maximum_height = 0;
  const aura::WindowDelegate* delegate(window->delegate());
  if (delegate) {
    if (delegate->GetMinimumSize().height() != 0) {
      minimum_height = std::max(kMinimumHeight,
                                delegate->GetMinimumSize().height());
    }
    if (delegate->GetMaximumSize().height() != 0)
      maximum_height = delegate->GetMaximumSize().height();
  }
  if (minimum_height)
    target_height = std::max(target_height, minimum_height);
  if (maximum_height)
    target_height = std::min(target_height, maximum_height);
  return target_height;
}

}  // namespace

struct DockedWindowLayoutManager::WindowWithHeight {
  explicit WindowWithHeight(aura::Window* window)
      : window(window), height(window->bounds().height()) {}
  aura::Window* window;
  int height;
};

// A functor used to sort the windows in order of their minimum height.
struct DockedWindowLayoutManager::CompareMinimumHeight {
  bool operator()(const WindowWithHeight& win1, const WindowWithHeight& win2) {
    return GetWindowHeightCloseTo(win1.window, 0) <
           GetWindowHeightCloseTo(win2.window, 0);
  }
};

// A functor used to sort the windows in order of their center Y position.
// |delta| is a pre-calculated distance from the bottom of one window to the top
// of the next. Its value can be positive (gap) or negative (overlap).
// Half of |delta| is used as a transition point at which windows could ideally
// swap positions.
struct DockedWindowLayoutManager::CompareWindowPos {
  CompareWindowPos(aura::Window* dragged_window,
                   aura::Window* docked_container,
                   float delta)
      : dragged_window_(dragged_window),
        docked_container_(docked_container),
        delta_(delta / 2) {}

  bool operator()(const WindowWithHeight& window_with_height1,
                  const WindowWithHeight& window_with_height2) {
    // Use target coordinates since animations may be active when windows are
    // reordered.
    aura::Window* win1(window_with_height1.window);
    aura::Window* win2(window_with_height2.window);
    gfx::Rect win1_bounds = ScreenUtil::ConvertRectToScreen(
        docked_container_, win1->GetTargetBounds());
    gfx::Rect win2_bounds = ScreenUtil::ConvertRectToScreen(
        docked_container_, win2->GetTargetBounds());
    win1_bounds.set_height(window_with_height1.height);
    win2_bounds.set_height(window_with_height2.height);
    // If one of the windows is the |dragged_window_| attempt to make an
    // earlier swap between the windows than just based on their centers.
    // This is possible if the dragged window is at least as tall as the other
    // window.
    if (win1 == dragged_window_)
      return compare_two_windows(win1_bounds, win2_bounds);
    if (win2 == dragged_window_)
      return !compare_two_windows(win2_bounds, win1_bounds);
    // Otherwise just compare the centers.
    return win1_bounds.CenterPoint().y() < win2_bounds.CenterPoint().y();
  }

  // Based on center point tries to deduce where the drag is coming from.
  // When dragging from below up the transition point is lower.
  // When dragging from above down the transition point is higher.
  bool compare_bounds(const gfx::Rect dragged, const gfx::Rect other) {
    if (dragged.CenterPoint().y() < other.CenterPoint().y())
      return dragged.CenterPoint().y() < other.y() - delta_;
    return dragged.CenterPoint().y() < other.bottom() + delta_;
  }

  // Performs comparison both ways and selects stable result.
  bool compare_two_windows(const gfx::Rect bounds1, const gfx::Rect bounds2) {
    // Try comparing windows in both possible orders and see if the comparison
    // is stable.
    bool result1 = compare_bounds(bounds1, bounds2);
    bool result2 = compare_bounds(bounds2, bounds1);
    if (result1 != result2)
      return result1;

    // Otherwise it is not possible to be sure that the windows will not bounce.
    // In this case just compare the centers.
    return bounds1.CenterPoint().y() < bounds2.CenterPoint().y();
  }

 private:
  aura::Window* dragged_window_;
  aura::Window* docked_container_;
  float delta_;
};

////////////////////////////////////////////////////////////////////////////////
// A class that observes shelf for bounds changes.
class DockedWindowLayoutManager::ShelfWindowObserver : public WindowObserver {
 public:
  explicit ShelfWindowObserver(
      DockedWindowLayoutManager* docked_layout_manager)
      : docked_layout_manager_(docked_layout_manager) {
    DCHECK(docked_layout_manager_->shelf()->shelf_widget());
    docked_layout_manager_->shelf()->shelf_widget()->GetNativeView()
        ->AddObserver(this);
  }

  ~ShelfWindowObserver() override {
    if (docked_layout_manager_->shelf() &&
        docked_layout_manager_->shelf()->shelf_widget())
      docked_layout_manager_->shelf()->shelf_widget()->GetNativeView()
          ->RemoveObserver(this);
  }

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override {
    shelf_bounds_in_screen_ = ScreenUtil::ConvertRectToScreen(
        window->parent(), new_bounds);
    docked_layout_manager_->OnShelfBoundsChanged();
  }

  const gfx::Rect& shelf_bounds_in_screen() const {
    return shelf_bounds_in_screen_;
  }

 private:
  DockedWindowLayoutManager* docked_layout_manager_;
  gfx::Rect shelf_bounds_in_screen_;

  DISALLOW_COPY_AND_ASSIGN(ShelfWindowObserver);
};

////////////////////////////////////////////////////////////////////////////////
// DockedWindowLayoutManager public implementation:
DockedWindowLayoutManager::DockedWindowLayoutManager(
    aura::Window* dock_container,
    WorkspaceController* workspace_controller)
    : SnapToPixelLayoutManager(dock_container),
      dock_container_(dock_container),
      in_layout_(false),
      dragged_window_(nullptr),
      is_dragged_window_docked_(false),
      is_dragged_from_dock_(false),
      shelf_(nullptr),
      workspace_controller_(workspace_controller),
      in_fullscreen_(workspace_controller_->GetWindowState() ==
                     wm::WORKSPACE_WINDOW_STATE_FULL_SCREEN),
      docked_width_(0),
      alignment_(DOCKED_ALIGNMENT_NONE),
      preferred_alignment_(DOCKED_ALIGNMENT_NONE),
      event_source_(DOCKED_ACTION_SOURCE_UNKNOWN),
      last_active_window_(nullptr),
      last_action_time_(base::Time::Now()),
      background_widget_(nullptr) {
  DCHECK(dock_container);
  aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      AddObserver(this);
  Shell::GetInstance()->AddShellObserver(this);
}

DockedWindowLayoutManager::~DockedWindowLayoutManager() {
  Shutdown();
}

void DockedWindowLayoutManager::Shutdown() {
  background_widget_.reset();
  shelf_observer_.reset();
  shelf_ = nullptr;
  for (size_t i = 0; i < dock_container_->children().size(); ++i) {
    aura::Window* child = dock_container_->children()[i];
    child->RemoveObserver(this);
    wm::GetWindowState(child)->RemoveObserver(this);
  }
  aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())->
      RemoveObserver(this);
  Shell::GetInstance()->RemoveShellObserver(this);
}

void DockedWindowLayoutManager::AddObserver(
    DockedWindowLayoutManagerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void DockedWindowLayoutManager::RemoveObserver(
    DockedWindowLayoutManagerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void DockedWindowLayoutManager::StartDragging(aura::Window* window) {
  DCHECK(!dragged_window_);
  dragged_window_ = window;
  DCHECK(!IsPopupOrTransient(window));
  // Start observing a window unless it is docked container's child in which
  // case it is already observed.
  wm::WindowState* dragged_state = wm::GetWindowState(dragged_window_);
  if (dragged_window_->parent() != dock_container_) {
    dragged_window_->AddObserver(this);
    dragged_state->AddObserver(this);
  } else if (!IsAnyWindowDocked() &&
             dragged_state->drag_details() &&
             !(dragged_state->drag_details()->bounds_change &
                 WindowResizer::kBoundsChange_Resizes)) {
    // If there are no other docked windows clear alignment when a docked window
    // is moved (but not when it is resized or the window could get undocked
    // when resized away from the edge while docked).
    alignment_ = DOCKED_ALIGNMENT_NONE;
  }
  is_dragged_from_dock_ = window->parent() == dock_container_;
  DCHECK(!is_dragged_window_docked_);

  // Resize all windows that are flush with the dock edge together if one of
  // them gets resized.
  if (dragged_window_->bounds().width() == docked_width_ &&
      (dragged_state->drag_details()->bounds_change &
          WindowResizer::kBoundsChange_Resizes) &&
      (dragged_state->drag_details()->size_change_direction &
          WindowResizer::kBoundsChangeDirection_Horizontal)) {
    for (size_t i = 0; i < dock_container_->children().size(); ++i) {
      aura::Window* window1(dock_container_->children()[i]);
      if (IsWindowDocked(window1) && window1 != dragged_window_ &&
          window1->bounds().width() == docked_width_) {
        wm::GetWindowState(window1)->set_bounds_changed_by_user(false);
      }
    }
  }
}

void DockedWindowLayoutManager::DockDraggedWindow(aura::Window* window) {
  DCHECK(!IsPopupOrTransient(window));
  OnDraggedWindowDocked(window);
  Relayout();
}

void DockedWindowLayoutManager::UndockDraggedWindow() {
  DCHECK(!IsPopupOrTransient(dragged_window_));
  OnDraggedWindowUndocked();
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::CHILD_CHANGED);
  is_dragged_from_dock_ = false;
}

void DockedWindowLayoutManager::FinishDragging(DockedAction action,
                                               DockedActionSource source) {
  DCHECK(dragged_window_);
  DCHECK(!IsPopupOrTransient(dragged_window_));
  if (is_dragged_window_docked_)
    OnDraggedWindowUndocked();
  DCHECK (!is_dragged_window_docked_);
  // Stop observing a window unless it is docked container's child in which
  // case it needs to keep being observed after the drag completes.
  if (dragged_window_->parent() != dock_container_) {
    dragged_window_->RemoveObserver(this);
    wm::GetWindowState(dragged_window_)->RemoveObserver(this);
    if (last_active_window_ == dragged_window_)
      last_active_window_ = nullptr;
  } else {
    // If this is the first window that got docked by a move update alignment.
    if (alignment_ == DOCKED_ALIGNMENT_NONE)
      alignment_ = GetEdgeNearestWindow(dragged_window_);
    // A window is no longer dragged and is a child.
    // When a window becomes a child at drag start this is
    // the only opportunity we will have to enforce a window
    // count limit so do it here.
    MaybeMinimizeChildrenExcept(dragged_window_);
  }
  dragged_window_ = nullptr;
  dragged_bounds_ = gfx::Rect();
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::CHILD_CHANGED);
  RecordUmaAction(action, source);
}

void DockedWindowLayoutManager::SetShelf(Shelf* shelf) {
  DCHECK(!shelf_);
  shelf_ = shelf;
  shelf_observer_.reset(new ShelfWindowObserver(this));
}

DockedAlignment DockedWindowLayoutManager::GetAlignmentOfWindow(
    const aura::Window* window) const {
  const gfx::Rect& bounds(window->GetBoundsInScreen());

  // Test overlap with an existing docked area first.
  if (docked_bounds_.Intersects(bounds) &&
      alignment_ != DOCKED_ALIGNMENT_NONE) {
    // A window is being added to other docked windows (on the same side).
    return alignment_;
  }

  const gfx::Rect container_bounds = dock_container_->GetBoundsInScreen();
  if (bounds.x() <= container_bounds.x() &&
      bounds.right() > container_bounds.x()) {
    return DOCKED_ALIGNMENT_LEFT;
  } else if (bounds.x() < container_bounds.right() &&
             bounds.right() >= container_bounds.right()) {
    return DOCKED_ALIGNMENT_RIGHT;
  }
  return DOCKED_ALIGNMENT_NONE;
}

DockedAlignment DockedWindowLayoutManager::CalculateAlignment() const {
  return CalculateAlignmentExcept(dragged_window_);
}

DockedAlignment DockedWindowLayoutManager::CalculateAlignmentExcept(
    const aura::Window* window) const {
  // Find a child that is not the window being queried and is not a popup.
  // If such exists the current alignment is returned - even if some of the
  // children are hidden or minimized (so they can be restored without losing
  // the docked state).
  for (size_t i = 0; i < dock_container_->children().size(); ++i) {
    aura::Window* child(dock_container_->children()[i]);
    if (window != child && !IsPopupOrTransient(child))
      return alignment_;
  }
  // No docked windows remain other than possibly the window being queried.
  // Return |NONE| to indicate that windows may get docked on either side.
  return DOCKED_ALIGNMENT_NONE;
}

bool DockedWindowLayoutManager::CanDockWindow(
    aura::Window* window,
    DockedAlignment desired_alignment) {
  // Don't allow interactive docking of windows with transient parents such as
  // modal browser dialogs. Prevent docking of panels attached to shelf during
  // the drag.
  wm::WindowState* window_state = wm::GetWindowState(window);
  bool should_attach_to_shelf = window_state->drag_details() &&
      window_state->drag_details()->should_attach_to_shelf;
  if (IsPopupOrTransient(window) || should_attach_to_shelf)
    return false;
  // If a window is wide and cannot be resized down to maximum width allowed
  // then it cannot be docked.
  // TODO(varkha). Prevent windows from changing size programmatically while
  // they are docked. The size will take effect only once a window is undocked.
  // See http://crbug.com/307792.
  if (window->bounds().width() > kMaxDockWidth &&
      (!window_state->CanResize() ||
       (window->delegate() &&
        window->delegate()->GetMinimumSize().width() != 0 &&
        window->delegate()->GetMinimumSize().width() > kMaxDockWidth))) {
    return false;
  }
  // If a window is tall and cannot be resized down to maximum height allowed
  // then it cannot be docked.
  const gfx::Rect work_area = gfx::Screen::GetScreen()
                                  ->GetDisplayNearestWindow(dock_container_)
                                  .work_area();
  if (GetWindowHeightCloseTo(window, work_area.height()) > work_area.height())
    return false;
  // Cannot dock on the other size from an existing dock.
  const DockedAlignment alignment = CalculateAlignmentExcept(window);
  if (desired_alignment != DOCKED_ALIGNMENT_NONE &&
      alignment != DOCKED_ALIGNMENT_NONE &&
      alignment != desired_alignment) {
    return false;
  }
  // Do not allow docking on the same side as shelf.
  return IsDockedAlignmentValid(desired_alignment);
}

bool DockedWindowLayoutManager::IsDockedAlignmentValid(
    DockedAlignment alignment) const {
  ShelfAlignment shelf_alignment = shelf_ ? shelf_->alignment() :
      SHELF_ALIGNMENT_BOTTOM;
  if ((alignment == DOCKED_ALIGNMENT_LEFT &&
       shelf_alignment == SHELF_ALIGNMENT_LEFT) ||
      (alignment == DOCKED_ALIGNMENT_RIGHT &&
       shelf_alignment == SHELF_ALIGNMENT_RIGHT)) {
    return false;
  }
  return true;
}

void DockedWindowLayoutManager::MaybeSetDesiredDockedAlignment(
    DockedAlignment alignment) {
  // If the requested alignment is |NONE| or there are no
  // docked windows return early as we can't change whether there is a
  // dock or not. If the requested alignment is the same as the current
  // alignment return early as an optimization.
  if (alignment == DOCKED_ALIGNMENT_NONE ||
      alignment_ == DOCKED_ALIGNMENT_NONE ||
      alignment_ == alignment ||
      !IsDockedAlignmentValid(alignment)) {
    return;
  }
  alignment_ = alignment;

  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::CHILD_CHANGED);
}

void DockedWindowLayoutManager::OnShelfBoundsChanged() {
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::DISPLAY_INSETS_CHANGED);
}

////////////////////////////////////////////////////////////////////////////////
// DockedWindowLayoutManager, aura::LayoutManager implementation:
void DockedWindowLayoutManager::OnWindowResized() {
  MaybeMinimizeChildrenExcept(dragged_window_);
  Relayout();
  // When screen resizes update the insets even when dock width or alignment
  // does not change.
  UpdateDockBounds(DockedWindowLayoutManagerObserver::DISPLAY_RESIZED);
}

void DockedWindowLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  if (IsPopupOrTransient(child))
    return;
  // Dragged windows are already observed by StartDragging and do not change
  // docked alignment during the drag.
  if (child == dragged_window_)
    return;
  // If this is the first window getting docked - update alignment.
  // A window can be added without proper bounds when window is moved to another
  // display via API or due to display configuration change, so the alignment
  // is set based on which edge is closer in the new display.
  if (alignment_ == DOCKED_ALIGNMENT_NONE) {
    alignment_ = preferred_alignment_ != DOCKED_ALIGNMENT_NONE ?
        preferred_alignment_ : GetEdgeNearestWindow(child);
  }
  MaybeMinimizeChildrenExcept(child);
  child->AddObserver(this);
  wm::GetWindowState(child)->AddObserver(this);
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::CHILD_CHANGED);

  // Only keyboard-initiated actions are recorded here. Dragging cases
  // are handled in FinishDragging.
  if (event_source_ != DOCKED_ACTION_SOURCE_UNKNOWN)
    RecordUmaAction(DOCKED_ACTION_DOCK, event_source_);
}

void DockedWindowLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
  if (IsPopupOrTransient(child))
    return;
  // Dragged windows are stopped being observed by FinishDragging and do not
  // change alignment during the drag. They also cannot be set to be the
  // |last_active_window_|.
  if (child == dragged_window_)
    return;
  // If this is the last window, set alignment and maximize the workspace.
  if (!IsAnyWindowDocked()) {
    alignment_ = DOCKED_ALIGNMENT_NONE;
    UpdateDockedWidth(0);
  }
  if (last_active_window_ == child)
    last_active_window_ = nullptr;
  child->RemoveObserver(this);
  wm::GetWindowState(child)->RemoveObserver(this);
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::CHILD_CHANGED);
}

void DockedWindowLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
  if (IsPopupOrTransient(child))
    return;

  wm::WindowState* window_state = wm::GetWindowState(child);
  if (visible && window_state->IsMinimized())
    window_state->Restore();
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::CHILD_CHANGED);
}

void DockedWindowLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  // The minimum constraints have to be applied first by the layout manager.
  gfx::Rect actual_new_bounds(requested_bounds);
  if (child->delegate()) {
    const gfx::Size& min_size = child->delegate()->GetMinimumSize();
    actual_new_bounds.set_width(
        std::max(min_size.width(), actual_new_bounds.width()));
    actual_new_bounds.set_height(
        std::max(min_size.height(), actual_new_bounds.height()));
  }
  if (IsWindowDocked(child) && child != dragged_window_)
    return;
  SnapToPixelLayoutManager::SetChildBounds(child, actual_new_bounds);
  if (IsPopupOrTransient(child))
    return;
  // Whenever one of our windows is moved or resized enforce layout.
  ShelfLayoutManager* shelf_layout = shelf_->shelf_layout_manager();
  if (shelf_layout)
    shelf_layout->UpdateVisibilityState();
}

////////////////////////////////////////////////////////////////////////////////
// DockedWindowLayoutManager, ash::ShellObserver implementation:

void DockedWindowLayoutManager::OnDisplayWorkAreaInsetsChanged() {
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::DISPLAY_INSETS_CHANGED);
  MaybeMinimizeChildrenExcept(dragged_window_);
}

void DockedWindowLayoutManager::OnFullscreenStateChanged(
    bool is_fullscreen, aura::Window* root_window) {
  if (dock_container_->GetRootWindow() != root_window)
    return;
  // Entering fullscreen mode (including immersive) hides docked windows.
  in_fullscreen_ = workspace_controller_->GetWindowState() ==
                   wm::WORKSPACE_WINDOW_STATE_FULL_SCREEN;
  {
    // prevent Relayout from getting called multiple times during this
    base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);
    // Use a copy of children array because a call to MinimizeDockedWindow or
    // RestoreDockedWindow can change order.
    aura::Window::Windows children(dock_container_->children());
    for (aura::Window::Windows::const_iterator iter = children.begin();
         iter != children.end(); ++iter) {
      aura::Window* window(*iter);
      if (IsPopupOrTransient(window))
        continue;
      wm::WindowState* window_state = wm::GetWindowState(window);
      if (in_fullscreen_) {
        if (window->IsVisible())
          MinimizeDockedWindow(window_state);
      } else {
        if (!window_state->IsMinimized())
          RestoreDockedWindow(window_state);
      }
    }
  }
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::CHILD_CHANGED);
}

void DockedWindowLayoutManager::OnShelfAlignmentChanged(
    aura::Window* root_window) {
  if (dock_container_->GetRootWindow() != root_window)
    return;

  if (!shelf_ || alignment_ == DOCKED_ALIGNMENT_NONE)
    return;

  // Do not allow shelf and dock on the same side. Switch side that
  // the dock is attached to and move all dock windows to that new side.
  ShelfAlignment shelf_alignment = shelf_->alignment();
  if (alignment_ == DOCKED_ALIGNMENT_LEFT &&
      shelf_alignment == SHELF_ALIGNMENT_LEFT) {
    alignment_ = DOCKED_ALIGNMENT_RIGHT;
  } else if (alignment_ == DOCKED_ALIGNMENT_RIGHT &&
             shelf_alignment == SHELF_ALIGNMENT_RIGHT) {
    alignment_ = DOCKED_ALIGNMENT_LEFT;
  }
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::SHELF_ALIGNMENT_CHANGED);
}

/////////////////////////////////////////////////////////////////////////////
// DockedWindowLayoutManager, WindowStateObserver implementation:

void DockedWindowLayoutManager::OnPreWindowStateTypeChange(
    wm::WindowState* window_state,
    wm::WindowStateType old_type) {
  aura::Window* window = window_state->aura_window();
  if (IsPopupOrTransient(window))
    return;
  // The window property will still be set, but no actual change will occur
  // until OnFullscreenStateChange is called when exiting fullscreen.
  if (in_fullscreen_)
    return;
  if (!window_state->IsDocked()) {
    if (window != dragged_window_) {
      UndockWindow(window);
      if (window_state->IsMaximizedOrFullscreen())
        RecordUmaAction(DOCKED_ACTION_MAXIMIZE, event_source_);
      else
        RecordUmaAction(DOCKED_ACTION_UNDOCK, event_source_);
    }
  } else if (window_state->IsMinimized()) {
    MinimizeDockedWindow(window_state);
  } else if (old_type == wm::WINDOW_STATE_TYPE_DOCKED_MINIMIZED) {
    RestoreDockedWindow(window_state);
  } else if (old_type == wm::WINDOW_STATE_TYPE_MINIMIZED) {
    NOTREACHED() << "Minimized window in docked layout manager";
  }
}

/////////////////////////////////////////////////////////////////////////////
// DockedWindowLayoutManager, WindowObserver implementation:

void DockedWindowLayoutManager::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  // Only relayout if the dragged window would get docked.
  if (window == dragged_window_ && is_dragged_window_docked_)
    Relayout();
}

void DockedWindowLayoutManager::OnWindowVisibilityChanging(
    aura::Window* window, bool visible) {
  if (IsPopupOrTransient(window))
    return;
  int animation_type = ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT;
  if (visible) {
    animation_type = ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_DROP;
    ::wm::SetWindowVisibilityAnimationDuration(
        window, base::TimeDelta::FromMilliseconds(kFadeDurationMs));
  } else if (wm::GetWindowState(window)->IsMinimized()) {
    animation_type = wm::WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE;
  }
  ::wm::SetWindowVisibilityAnimationType(window, animation_type);
}

void DockedWindowLayoutManager::OnWindowDestroying(aura::Window* window) {
  if (dragged_window_ == window) {
    FinishDragging(DOCKED_ACTION_NONE, DOCKED_ACTION_SOURCE_UNKNOWN);
    DCHECK(!dragged_window_);
    DCHECK(!is_dragged_window_docked_);
  }
  if (window == last_active_window_)
    last_active_window_ = nullptr;
  RecordUmaAction(DOCKED_ACTION_CLOSE, event_source_);
}

////////////////////////////////////////////////////////////////////////////////
// DockedWindowLayoutManager, aura::client::ActivationChangeObserver
// implementation:

void DockedWindowLayoutManager::OnWindowActivated(
    aura::client::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (gained_active && IsPopupOrTransient(gained_active))
    return;
  // Ignore if the window that is not managed by this was activated.
  aura::Window* ancestor = nullptr;
  for (aura::Window* parent = gained_active;
       parent; parent = parent->parent()) {
    if (parent->parent() == dock_container_) {
      ancestor = parent;
      break;
    }
  }
  if (ancestor)
    UpdateStacking(ancestor);
}

////////////////////////////////////////////////////////////////////////////////
// DockedWindowLayoutManager private implementation:

void DockedWindowLayoutManager::MaybeMinimizeChildrenExcept(
    aura::Window* child) {
  // Minimize any windows that don't fit without overlap.
  const gfx::Rect work_area = gfx::Screen::GetScreen()
                                  ->GetDisplayNearestWindow(dock_container_)
                                  .work_area();
  int available_room = work_area.height();
  bool gap_needed = !!child;
  if (child)
    available_room -= GetWindowHeightCloseTo(child, 0);
  // Use a copy of children array because a call to Minimize can change order.
  aura::Window::Windows children(dock_container_->children());
  aura::Window::Windows::const_reverse_iterator iter = children.rbegin();
  while (iter != children.rend()) {
    aura::Window* window(*iter++);
    if (window == child || !IsWindowDocked(window))
      continue;
    int room_needed = GetWindowHeightCloseTo(window, 0) +
        (gap_needed ? kMinDockGap : 0);
    gap_needed = true;
    if (available_room > room_needed) {
      available_room -= room_needed;
    } else {
      // Slow down minimizing animations. Lock duration so that it is not
      // overridden by other ScopedLayerAnimationSettings down the stack.
      ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
      settings.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kMinimizeDurationMs));
      settings.LockTransitionDuration();
      wm::GetWindowState(window)->Minimize();
    }
  }
}

void DockedWindowLayoutManager::MinimizeDockedWindow(
    wm::WindowState* window_state) {
  DCHECK(!IsPopupOrTransient(window_state->aura_window()));
  window_state->window()->Hide();
  if (window_state->IsActive())
    window_state->Deactivate();
  RecordUmaAction(DOCKED_ACTION_MINIMIZE, event_source_);
}

void DockedWindowLayoutManager::RestoreDockedWindow(
    wm::WindowState* window_state) {
  aura::Window* window = window_state->aura_window();
  DCHECK(!IsPopupOrTransient(window));
  // Always place restored window at the bottom shuffling the other windows up.
  // TODO(varkha): add a separate container for docked windows to keep track
  // of ordering.
  gfx::Display display =
      gfx::Screen::GetScreen()->GetDisplayNearestWindow(dock_container_);
  const gfx::Rect work_area = display.work_area();

  // Evict the window if it can no longer be docked because of its height.
  if (!CanDockWindow(window, DOCKED_ALIGNMENT_NONE)) {
    window_state->Restore();
    RecordUmaAction(DOCKED_ACTION_EVICT, event_source_);
    return;
  }
  gfx::Rect bounds(window->bounds());
  bounds.set_y(work_area.bottom());
  window->SetBounds(bounds);
  window->Show();
  MaybeMinimizeChildrenExcept(window);
  RecordUmaAction(DOCKED_ACTION_RESTORE, event_source_);
}

void DockedWindowLayoutManager::RecordUmaAction(DockedAction action,
                                                DockedActionSource source) {
  if (action == DOCKED_ACTION_NONE)
    return;
  UMA_HISTOGRAM_ENUMERATION("Ash.Dock.Action", action, DOCKED_ACTION_COUNT);
  UMA_HISTOGRAM_ENUMERATION("Ash.Dock.ActionSource", source,
                            DOCKED_ACTION_SOURCE_COUNT);
  base::Time time_now = base::Time::Now();
  base::TimeDelta time_between_use = time_now - last_action_time_;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.Dock.TimeBetweenUse",
                              time_between_use.InSeconds(),
                              1,
                              base::TimeDelta::FromHours(10).InSeconds(),
                              100);
  last_action_time_ = time_now;
  int docked_all_count = 0;
  int docked_visible_count = 0;
  int docked_panels_count = 0;
  int large_windows_count = 0;
  for (size_t i = 0; i < dock_container_->children().size(); ++i) {
    const aura::Window* window(dock_container_->children()[i]);
    if (IsPopupOrTransient(window))
      continue;
    docked_all_count++;
    if (!IsWindowDocked(window))
      continue;
    docked_visible_count++;
    if (window->type() == ui::wm::WINDOW_TYPE_PANEL)
      docked_panels_count++;
    const wm::WindowState* window_state = wm::GetWindowState(window);
    if (window_state->HasRestoreBounds()) {
      const gfx::Rect restore_bounds = window_state->GetRestoreBoundsInScreen();
      if (restore_bounds.width() > kMaxDockWidth)
        large_windows_count++;
    }
  }
  UMA_HISTOGRAM_COUNTS_100("Ash.Dock.ItemsAll", docked_all_count);
  UMA_HISTOGRAM_COUNTS_100("Ash.Dock.ItemsLarge", large_windows_count);
  UMA_HISTOGRAM_COUNTS_100("Ash.Dock.ItemsPanels", docked_panels_count);
  UMA_HISTOGRAM_COUNTS_100("Ash.Dock.ItemsVisible", docked_visible_count);
}

void DockedWindowLayoutManager::UpdateDockedWidth(int width) {
  if (docked_width_ == width)
    return;
  docked_width_ = width;
  UMA_HISTOGRAM_COUNTS_10000("Ash.Dock.Width", docked_width_);
}

void DockedWindowLayoutManager::OnDraggedWindowDocked(aura::Window* window) {
  DCHECK(!is_dragged_window_docked_);
  is_dragged_window_docked_ = true;
}

void DockedWindowLayoutManager::OnDraggedWindowUndocked() {
  DCHECK (is_dragged_window_docked_);
  is_dragged_window_docked_ = false;
}

bool DockedWindowLayoutManager::IsAnyWindowDocked() {
  return CalculateAlignment() != DOCKED_ALIGNMENT_NONE;
}

DockedAlignment DockedWindowLayoutManager::GetEdgeNearestWindow(
    const aura::Window* window) const {
  const gfx::Rect& bounds(window->GetBoundsInScreen());
  const gfx::Rect container_bounds = dock_container_->GetBoundsInScreen();
  // Give one pixel preference for docking on the right side to a window that
  // has odd width and is centered in a screen that has even width (or vice
  // versa). This only matters to the tests but could be a source of flakiness.
  return (abs(bounds.x() - container_bounds.x()) + 1 <
          abs(bounds.right() - container_bounds.right()))
             ? DOCKED_ALIGNMENT_LEFT
             : DOCKED_ALIGNMENT_RIGHT;
}

void DockedWindowLayoutManager::Relayout() {
  if (in_layout_)
    return;
  if (alignment_ == DOCKED_ALIGNMENT_NONE && !is_dragged_window_docked_)
    return;
  base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);

  gfx::Rect dock_bounds = dock_container_->GetBoundsInScreen();
  aura::Window* active_window = nullptr;
  std::vector<WindowWithHeight> visible_windows;
  for (size_t i = 0; i < dock_container_->children().size(); ++i) {
    aura::Window* window(dock_container_->children()[i]);

    if (!IsWindowDocked(window) || window == dragged_window_)
      continue;

    // If the shelf is currently hidden (full-screen mode), hide window until
    // full-screen mode is exited.
    if (in_fullscreen_) {
      // The call to Hide does not set the minimize property, so the window will
      // be restored when the shelf becomes visible again.
      window->Hide();
      continue;
    }
    if (window->HasFocus() ||
        window->Contains(
            aura::client::GetFocusClient(window)->GetFocusedWindow())) {
      DCHECK(!active_window);
      active_window = window;
    }
    visible_windows.push_back(WindowWithHeight(window));
  }
  // Consider docked dragged_window_ when fanning out other child windows.
  if (is_dragged_window_docked_) {
    visible_windows.push_back(WindowWithHeight(dragged_window_));
    DCHECK(!active_window);
    active_window = dragged_window_;
  }

  // Position docked windows as well as the window being dragged.
  gfx::Rect work_area = gfx::Screen::GetScreen()
                            ->GetDisplayNearestWindow(dock_container_)
                            .work_area();
  if (shelf_observer_)
    work_area.Subtract(shelf_observer_->shelf_bounds_in_screen());
  int available_room = CalculateWindowHeightsAndRemainingRoom(work_area,
                                                              &visible_windows);
  FanOutChildren(work_area,
                 CalculateIdealWidth(visible_windows),
                 available_room,
                 &visible_windows);

  // After the first Relayout allow the windows to change their order easier
  // since we know they are docked.
  is_dragged_from_dock_ = true;
  UpdateStacking(active_window);
}

int DockedWindowLayoutManager::CalculateWindowHeightsAndRemainingRoom(
    const gfx::Rect work_area,
    std::vector<WindowWithHeight>* visible_windows) {
  int available_room = work_area.height();
  int remaining_windows = visible_windows->size();
  int gap_height = remaining_windows > 1 ? kMinDockGap : 0;

  // Sort windows by their minimum heights and calculate target heights.
  std::sort(visible_windows->begin(), visible_windows->end(),
            CompareMinimumHeight());
  // Distribute the free space among the docked windows. Since the windows are
  // sorted (tall windows first) we can now assume that any window which
  // required more space than the current window will have already been
  // accounted for previously in this loop, so we can safely give that window
  // its proportional share of the remaining space.
  for (std::vector<WindowWithHeight>::reverse_iterator iter =
           visible_windows->rbegin();
      iter != visible_windows->rend(); ++iter) {
    iter->height = GetWindowHeightCloseTo(
        iter->window,
        (available_room + gap_height) / remaining_windows - gap_height);
    available_room -= (iter->height + gap_height);
    remaining_windows--;
  }
  return available_room + gap_height;
}

int DockedWindowLayoutManager::CalculateIdealWidth(
    const std::vector<WindowWithHeight>& visible_windows) {
  int smallest_max_width = kMaxDockWidth;
  int largest_min_width = kMinDockWidth;
  // Ideal width of the docked area is as close to kIdealWidth as possible
  // while still respecting the minimum and maximum width restrictions on the
  // individual docked windows as well as the width that was possibly set by a
  // user (which needs to be preserved when dragging and rearranging windows).
  for (std::vector<WindowWithHeight>::const_iterator iter =
           visible_windows.begin();
       iter != visible_windows.end(); ++iter) {
    const aura::Window* window = iter->window;
    int min_window_width = window->bounds().width();
    int max_window_width = min_window_width;
    if (!wm::GetWindowState(window)->bounds_changed_by_user()) {
      min_window_width = GetWindowWidthCloseTo(window, kMinDockWidth);
      max_window_width = GetWindowWidthCloseTo(window, kMaxDockWidth);
    }
    largest_min_width = std::max(largest_min_width, min_window_width);
    smallest_max_width = std::min(smallest_max_width, max_window_width);
  }
  int ideal_width = std::max(largest_min_width,
                             std::min(smallest_max_width, kIdealWidth));
  // Restrict docked area width regardless of window restrictions.
  ideal_width = std::max(std::min(ideal_width, kMaxDockWidth), kMinDockWidth);
  return ideal_width;
}

void DockedWindowLayoutManager::FanOutChildren(
    const gfx::Rect& work_area,
    int ideal_docked_width,
    int available_room,
    std::vector<WindowWithHeight>* visible_windows) {
  gfx::Rect dock_bounds = dock_container_->GetBoundsInScreen();

  // Calculate initial vertical offset and the gap or overlap between windows.
  const int num_windows = visible_windows->size();
  const float delta = static_cast<float>(available_room) /
      ((available_room > 0 || num_windows <= 1) ?
          num_windows + 1 : num_windows - 1);
  float y_pos = work_area.y() + ((delta > 0) ? delta : 0);

  // Docked area is shown only if there is at least one non-dragged visible
  // docked window.
  int new_width = ideal_docked_width;
  if (visible_windows->empty() ||
      (visible_windows->size() == 1 &&
       (*visible_windows)[0].window == dragged_window_)) {
    new_width = 0;
  }
  UpdateDockedWidth(new_width);
  // Sort windows by their center positions and fan out overlapping
  // windows.
  std::sort(visible_windows->begin(), visible_windows->end(),
            CompareWindowPos(is_dragged_from_dock_ ? dragged_window_ : nullptr,
                             dock_container_, delta));
  for (std::vector<WindowWithHeight>::iterator iter = visible_windows->begin();
       iter != visible_windows->end(); ++iter) {
    aura::Window* window = iter->window;
    gfx::Rect bounds = ScreenUtil::ConvertRectToScreen(
        dock_container_, window->GetTargetBounds());
    // A window is extended or shrunk to be as close as possible to the ideal
    // docked area width. Windows that were resized by a user are kept at their
    // existing size.
    // This also enforces the min / max restrictions on the docked area width.
    bounds.set_width(GetWindowWidthCloseTo(
        window,
        wm::GetWindowState(window)->bounds_changed_by_user() ?
            bounds.width() : ideal_docked_width));
    DCHECK_LE(bounds.width(), ideal_docked_width);

    DockedAlignment alignment = alignment_;
    if (alignment == DOCKED_ALIGNMENT_NONE && window == dragged_window_)
      alignment = GetEdgeNearestWindow(window);

    // Fan out windows evenly distributing the overlap or remaining free space.
    bounds.set_height(iter->height);
    bounds.set_y(std::max(work_area.y(),
                          std::min(work_area.bottom() - bounds.height(),
                                   static_cast<int>(y_pos + 0.5))));
    y_pos += bounds.height() + delta + kMinDockGap;

    // All docked windows other than the one currently dragged remain stuck
    // to the screen edge (flush with the edge or centered in the dock area).
    switch (alignment) {
      case DOCKED_ALIGNMENT_LEFT:
        bounds.set_x(dock_bounds.x() +
                     (ideal_docked_width - bounds.width()) / 2);
        break;
      case DOCKED_ALIGNMENT_RIGHT:
        bounds.set_x(dock_bounds.right() -
                     (ideal_docked_width + bounds.width()) / 2);
        break;
      case DOCKED_ALIGNMENT_NONE:
        break;
    }
    if (window == dragged_window_) {
      dragged_bounds_ = bounds;
      continue;
    }
    // If the following asserts it is probably because not all the children
    // have been removed when dock was closed.
    DCHECK_NE(alignment_, DOCKED_ALIGNMENT_NONE);
    bounds = ScreenUtil::ConvertRectFromScreen(dock_container_, bounds);
    if (bounds != window->GetTargetBounds()) {
      ui::Layer* layer = window->layer();
      ui::ScopedLayerAnimationSettings slide_settings(layer->GetAnimator());
      slide_settings.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      slide_settings.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kSlideDurationMs));
      SetChildBoundsDirect(window, bounds);
    }
  }
}

void DockedWindowLayoutManager::UpdateDockBounds(
    DockedWindowLayoutManagerObserver::Reason reason) {
  int dock_inset = docked_width_ + (docked_width_ > 0 ? kMinDockGap : 0);
  const gfx::Rect work_area = gfx::Screen::GetScreen()
                                  ->GetDisplayNearestWindow(dock_container_)
                                  .work_area();
  gfx::Rect bounds = gfx::Rect(
      alignment_ == DOCKED_ALIGNMENT_RIGHT && dock_inset > 0 ?
          dock_container_->bounds().right() - dock_inset:
          dock_container_->bounds().x(),
      dock_container_->bounds().y(),
      dock_inset,
      work_area.height());
  docked_bounds_ = bounds +
      dock_container_->GetBoundsInScreen().OffsetFromOrigin();
  FOR_EACH_OBSERVER(
      DockedWindowLayoutManagerObserver,
      observer_list_,
      OnDockBoundsChanging(bounds, reason));
  // Show or hide background for docked area.
  gfx::Rect background_bounds(docked_bounds_);
  if (shelf_observer_)
    background_bounds.Subtract(shelf_observer_->shelf_bounds_in_screen());
  if (docked_width_ > 0) {
    if (!background_widget_)
      background_widget_.reset(new DockedBackgroundWidget(this));
    background_widget_->SetBackgroundBounds(background_bounds, alignment_);
    background_widget_->Show();
  } else if (background_widget_) {
    background_widget_->Hide();
  }
}

void DockedWindowLayoutManager::UpdateStacking(aura::Window* active_window) {
  if (!active_window) {
    if (!last_active_window_)
      return;
    active_window = last_active_window_;
  }

  // Windows are stacked like a deck of cards:
  //  ,------.
  // |,------.|
  // |,------.|
  // | active |
  // | window |
  // |`------'|
  // |`------'|
  //  `------'
  // Use the middle of each window to figure out how to stack the window.
  // This allows us to update the stacking when a window is being dragged around
  // by the titlebar.
  std::map<int, aura::Window*> window_ordering;
  for (aura::Window::Windows::const_iterator it =
           dock_container_->children().begin();
       it != dock_container_->children().end(); ++it) {
    if (!IsWindowDocked(*it) ||
        ((*it) == dragged_window_ && !is_dragged_window_docked_)) {
      continue;
    }
    gfx::Rect bounds = (*it)->bounds();
    window_ordering.insert(std::make_pair(bounds.y() + bounds.height() / 2,
                                          *it));
  }
  int active_center_y = active_window->bounds().CenterPoint().y();

  aura::Window* previous_window = nullptr;
  for (std::map<int, aura::Window*>::const_iterator it =
       window_ordering.begin();
       it != window_ordering.end() && it->first < active_center_y; ++it) {
    if (previous_window)
      dock_container_->StackChildAbove(it->second, previous_window);
    previous_window = it->second;
  }
  for (std::map<int, aura::Window*>::const_reverse_iterator it =
       window_ordering.rbegin();
       it != window_ordering.rend() && it->first > active_center_y; ++it) {
    if (previous_window)
      dock_container_->StackChildAbove(it->second, previous_window);
    previous_window = it->second;
  }

  if (previous_window && active_window->parent() == dock_container_)
    dock_container_->StackChildAbove(active_window, previous_window);
  if (active_window != dragged_window_)
    last_active_window_ = active_window;
}

////////////////////////////////////////////////////////////////////////////////
// keyboard::KeyboardControllerObserver implementation:

void DockedWindowLayoutManager::OnKeyboardBoundsChanging(
    const gfx::Rect& keyboard_bounds) {
  // This bounds change will have caused a change to the Shelf which does not
  // propagate automatically to this class, so manually recalculate bounds.
  Relayout();
  UpdateDockBounds(DockedWindowLayoutManagerObserver::KEYBOARD_BOUNDS_CHANGING);
}

}  // namespace ash
