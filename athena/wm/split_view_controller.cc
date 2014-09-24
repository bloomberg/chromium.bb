// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/split_view_controller.h"

#include <cmath>

#include "athena/screen/public/screen_manager.h"
#include "athena/wm/public/window_list_provider.h"
#include "athena/wm/public/window_manager.h"
#include "base/bind.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/closure_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/root_view_targeter.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace athena {

namespace {

const int kDragHandleWidth = 4;
const int kDragHandleHeight = 80;
const int kDragHandleMargin = 1;
const int kDividerWidth = kDragHandleWidth + 2 * kDragHandleMargin;

// Always returns the same target.
class StaticViewTargeterDelegate : public views::ViewTargeterDelegate {
 public:
  explicit StaticViewTargeterDelegate(views::View* target) : target_(target) {}

  virtual ~StaticViewTargeterDelegate() {}

 private:
  // views::ViewTargeterDelegate:
  virtual views::View* TargetForRect(views::View* root,
                                     const gfx::Rect& rect) OVERRIDE {
    return target_;
  }

  // Not owned.
  views::View* target_;

  DISALLOW_COPY_AND_ASSIGN(StaticViewTargeterDelegate);
};

// Expands the effective target area of the window of the widget containing the
// specified view. If the view is large enough to begin with, there should be
// no change from the default targeting behavior.
class PriorityWindowTargeter : public aura::WindowTargeter,
                               public aura::WindowObserver {
 public:
  explicit PriorityWindowTargeter(views::View* priority_view)
      : priority_view_(priority_view) {
    CHECK(priority_view->GetWidget());
    window_ = priority_view->GetWidget()->GetNativeWindow();
    CHECK(window_);
    window_->AddObserver(this);
  }

  virtual ~PriorityWindowTargeter() {
    window_->RemoveObserver(this);
  }

 private:
  // aura::WindowTargeter:
  virtual ui::EventTarget* FindTargetForLocatedEvent(
      ui::EventTarget* root,
      ui::LocatedEvent* event) OVERRIDE {
    if (!window_ || (event->type() != ui::ET_TOUCH_PRESSED))
      return WindowTargeter::FindTargetForLocatedEvent(root, event);
    CHECK_EQ(window_, priority_view_->GetWidget()->GetNativeWindow());

    // Bounds of the view in root window's coordinates.
    gfx::Rect view_bounds = priority_view_->GetBoundsInScreen();
    // If there is a transform on the window's layer - apply it.
    gfx::Transform window_transform = window_->layer()->transform();
    gfx::RectF transformed_bounds_f = view_bounds;
    window_transform.TransformRect(&transformed_bounds_f);
    gfx::Rect transformed_bounds = gfx::Rect(transformed_bounds_f.x(),
                                             transformed_bounds_f.y(),
                                             transformed_bounds_f.width(),
                                             transformed_bounds_f.height());
    // Now expand the bounds to be at least
    // kMinTouchDimension x kMinTouchDimension and target the event to the
    // window if it falls within the expanded bounds
    gfx::Point center = transformed_bounds.CenterPoint();
    gfx::Rect extension_rect = gfx::Rect(
        center.x() - kMinTouchDimension / 2,
        center.y() - kMinTouchDimension / 2,
        kMinTouchDimension,
        kMinTouchDimension);
    gfx::Rect extended_bounds =
        gfx::UnionRects(transformed_bounds, extension_rect);
    if (extended_bounds.Contains(event->root_location())) {
      root->ConvertEventToTarget(window_, event);
      return window_;
    }

    return WindowTargeter::FindTargetForLocatedEvent(root, event);
  }

  // aura::WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    DCHECK_EQ(window, window_);
    window_->RemoveObserver(this);
    window_ = NULL;
  }

  // Minimum dimension of a target to be comfortably touchable.
  // The effective touch target area of |priority_window_| gets expanded so
  // that it's width and height is ayt least |kMinTouchDimension|.
  int const kMinTouchDimension = 26;

  aura::Window* window_;
  views::View* priority_view_;

  DISALLOW_COPY_AND_ASSIGN(PriorityWindowTargeter);
};

// Returns a target transform required to transform |from| to |to|.
gfx::Transform GetTransformForBounds(const gfx::Rect& from,
                                     const gfx::Rect& to) {
  gfx::Transform transform;
  transform.Translate(to.x() - from.x(), to.y() - from.y());
  transform.Scale(to.width() / static_cast<float>(from.width()),
                  to.height() / static_cast<float>(from.height()));
  return transform;
}

bool IsLandscapeOrientation(gfx::Display::Rotation rotation) {
  return rotation == gfx::Display::ROTATE_0 ||
         rotation == gfx::Display::ROTATE_180;
}

}  // namespace

SplitViewController::SplitViewController(
    aura::Window* container,
    WindowListProvider* window_list_provider)
    : state_(INACTIVE),
      container_(container),
      window_list_provider_(window_list_provider),
      left_window_(NULL),
      right_window_(NULL),
      divider_position_(0),
      divider_scroll_start_position_(0),
      divider_widget_(NULL),
      drag_handle_(NULL),
      weak_factory_(this) {
}

SplitViewController::~SplitViewController() {
}

bool SplitViewController::CanActivateSplitViewMode() const {
  // TODO(mfomitchev): return false in full screen.
  return (!IsSplitViewModeActive() &&
              window_list_provider_->GetWindowList().size() >= 2 &&
              IsLandscapeOrientation(gfx::Screen::GetNativeScreen()->
                  GetDisplayNearestWindow(container_).rotation()));
}

bool SplitViewController::IsSplitViewModeActive() const {
  return state_ == ACTIVE;
}

void SplitViewController::ActivateSplitMode(aura::Window* left,
                                            aura::Window* right,
                                            aura::Window* to_activate) {
  const aura::Window::Windows& windows = window_list_provider_->GetWindowList();
  aura::Window::Windows::const_reverse_iterator iter = windows.rbegin();
  if (state_ == ACTIVE) {
    if (!left && left_window_ != right)
      left = left_window_;
    if (!right && right_window_ != left)
      right = right_window_;
  }

  if (!left && iter != windows.rend()) {
    left = *iter;
    iter++;
    if (left == right && iter != windows.rend()) {
      left = *iter;
      iter++;
    }
  }

  if (!right && iter != windows.rend()) {
    right = *iter;
    iter++;
    if (right == left && iter != windows.rend()) {
      right = *iter;
      iter++;
    }
  }

  to_hide_.clear();
  if (left_window_ && left_window_ != left && left_window_ != right)
    to_hide_.push_back(left_window_);
  if (right_window_ && right_window_ != left && right_window_ != right)
    to_hide_.push_back(right_window_);

  left_window_ = left;
  right_window_ = right;

  divider_position_ = GetDefaultDividerPosition();
  SetState(ACTIVE);
  UpdateLayout(true);

  aura::client::ActivationClient* activation_client =
      aura::client::GetActivationClient(container_->GetRootWindow());
  aura::Window* active_window = activation_client->GetActiveWindow();
  if (to_activate) {
    CHECK(to_activate == left_window_ || to_activate == right_window_);
    wm::ActivateWindow(to_activate);
  } else if (active_window != left_window_ &&
             active_window != right_window_) {
    // A window which does not belong to an activity could be active.
    wm::ActivateWindow(left_window_);
  }
  active_window = activation_client->GetActiveWindow();

  if (active_window == left_window_)
    window_list_provider_->StackWindowBehindTo(right_window_, left_window_);
  else
    window_list_provider_->StackWindowBehindTo(left_window_, right_window_);
}

void SplitViewController::ReplaceWindow(aura::Window* window,
                                        aura::Window* replace_with) {
  CHECK(IsSplitViewModeActive());
  CHECK(replace_with);
  CHECK(window == left_window_ || window == right_window_);
  CHECK(replace_with != left_window_ && replace_with != right_window_);
  DCHECK(window_list_provider_->IsWindowInList(replace_with));

  aura::Window* not_replaced = NULL;
  if (window == left_window_) {
    left_window_ = replace_with;
    not_replaced = right_window_;
  } else {
    right_window_ = replace_with;
    not_replaced = left_window_;
  }
  UpdateLayout(false);

  wm::ActivateWindow(replace_with);
  window_list_provider_->StackWindowBehindTo(not_replaced, replace_with);

  window->SetTransform(gfx::Transform());
  window->Hide();
}

void SplitViewController::DeactivateSplitMode() {
  CHECK_EQ(ACTIVE, state_);
  SetState(INACTIVE);
  UpdateLayout(false);
  left_window_ = right_window_ = NULL;
}

void SplitViewController::InitializeDivider() {
  CHECK(!divider_widget_);
  CHECK(!drag_handle_);

  drag_handle_ = CreateDragHandleView(DRAG_HANDLE_HORIZONTAL,
                                      this,
                                      kDragHandleWidth,
                                      kDragHandleHeight);
  views::View* content_view = new views::View;
  content_view->set_background(
      views::Background::CreateSolidBackground(SK_ColorBLACK));
  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal,
                           kDragHandleMargin,
                           kDragHandleMargin,
                           0);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  content_view->SetLayoutManager(layout);
  content_view->AddChildView(drag_handle_);

  divider_widget_ = new views::Widget();
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.parent = container_;
  params.bounds = gfx::Rect(-kDividerWidth / 2,
                            0,
                            kDividerWidth,
                            container_->bounds().height());
  divider_widget_->Init(params);
  divider_widget_->SetContentsView(content_view);

  // Install a static view targeter on the root view which always targets
  // divider_view.
  // TODO(mfomitchev,tdanderson): This should not be needed:
  // 1. crbug.com/414339 - divider_view is the only view and it completely
  //    overlaps the root view.
  // 2. The logic in ViewTargeterDelegate::TargetForRect could be improved to
  //    work better for views that are narrow in one dimension and long in
  //    another dimension.
  views::internal::RootView* root_view =
      static_cast<views::internal::RootView*>(divider_widget_->GetRootView());
  view_targeter_delegate_.reset(new StaticViewTargeterDelegate(drag_handle_));
  views::ViewTargeter* targeter =
      new views::RootViewTargeter(view_targeter_delegate_.get(), root_view);
  divider_widget_->GetRootView()->SetEventTargeter(
      scoped_ptr<views::ViewTargeter>(targeter));
}

void SplitViewController::HideDivider() {
  divider_widget_->Hide();
  window_targeter_.reset();
}

void SplitViewController::ShowDivider() {
  divider_widget_->Show();
  if (!window_targeter_) {
    scoped_ptr<ui::EventTargeter> window_targeter =
        scoped_ptr<ui::EventTargeter>(new PriorityWindowTargeter(drag_handle_));
    window_targeter_.reset(
        new aura::ScopedWindowTargeter(container_, window_targeter.Pass()));
  }
}

gfx::Rect SplitViewController::GetLeftAreaBounds() {
  gfx::Rect work_area =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().work_area();
  return gfx::Rect(
      0, 0, divider_position_ - kDividerWidth / 2, work_area.height());
}

gfx::Rect SplitViewController::GetRightAreaBounds() {
  gfx::Rect work_area =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().work_area();
  int container_width = container_->bounds().width();
  return gfx::Rect(divider_position_ + kDividerWidth / 2,
                   0,
                   container_width - divider_position_ - kDividerWidth / 2,
                   work_area.height());
}

void SplitViewController::SetState(SplitViewController::State state) {
  if (state_ == state)
    return;

  if (divider_widget_ == NULL)
    InitializeDivider();

  state_ = state;

  ScreenManager::Get()->SetRotationLocked(state_ != INACTIVE);
  if (state == INACTIVE)
    HideDivider();
  else
    ShowDivider();
}

void SplitViewController::UpdateLayout(bool animate) {
  CHECK(left_window_);
  CHECK(right_window_);
  // Splitview can be activated from SplitViewController::ActivateSplitMode or
  // SplitViewController::ScrollEnd. Additionally we don't want to rotate the
  // screen while engaging splitview (i.e. state_ == SCROLLING).
  if (state_ == INACTIVE && !animate) {
    gfx::Rect work_area =
        gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().work_area();
    aura::Window* top_window = window_list_provider_->GetWindowList().back();
    if (top_window != left_window_) {
      // TODO(mfomitchev): Use to_hide_ instead
      left_window_->Hide();
      right_window_->SetBounds(gfx::Rect(work_area.size()));
    }
    if (top_window != right_window_) {
      left_window_->SetBounds(gfx::Rect(work_area.size()));
      // TODO(mfomitchev): Use to_hide_ instead
      right_window_->Hide();
    }
    SetWindowTransforms(
        gfx::Transform(), gfx::Transform(), gfx::Transform(), false);
    return;
  }

  left_window_->Show();
  right_window_->Show();

  gfx::Transform divider_transform;
  divider_transform.Translate(divider_position_, 0);
  if (state_ == ACTIVE) {
    if (animate) {
      gfx::Transform left_transform =
          GetTransformForBounds(left_window_->bounds(), GetLeftAreaBounds());
      gfx::Transform right_transform =
          GetTransformForBounds(right_window_->bounds(), GetRightAreaBounds());
      SetWindowTransforms(
          left_transform, right_transform, divider_transform, true);
    } else {
      left_window_->SetBounds(GetLeftAreaBounds());
      right_window_->SetBounds(GetRightAreaBounds());
      SetWindowTransforms(
          gfx::Transform(), gfx::Transform(), divider_transform, false);
    }
  } else {
    gfx::Transform left_transform;
    gfx::Transform right_transform;
    gfx::Rect left_area_bounds = GetLeftAreaBounds();
    gfx::Rect right_area_bounds = GetRightAreaBounds();
    // If the width of the window is greater than the width of the area which it
    // is supposed to occupy - translate the window. Otherwise scale the window
    // up to fill the target area.
    if (left_window_->bounds().width() >= left_area_bounds.width()) {
      left_transform.Translate(
          left_area_bounds.right() - left_window_->bounds().right(), 0);
    } else {
      left_transform =
          GetTransformForBounds(left_window_->bounds(), left_area_bounds);
    }
    if (right_window_->bounds().width() >= right_area_bounds.width()) {
      right_transform.Translate(
          right_area_bounds.x() - right_window_->bounds().x(), 0);
    } else {
      right_transform =
          GetTransformForBounds(right_window_->bounds(), right_area_bounds);
    }
    SetWindowTransforms(
        left_transform, right_transform, divider_transform, animate);
  }
  // Note: |left_window_| and |right_window_| may be NULL if calling
  // SetWindowTransforms():
  // - caused the in-progress animation to abort.
  // - started a zero duration animation.
}

void SplitViewController::SetWindowTransforms(
    const gfx::Transform& left_transform,
    const gfx::Transform& right_transform,
    const gfx::Transform& divider_transform,
    bool animate) {
  if (animate) {
    ui::ScopedLayerAnimationSettings left_settings(
        left_window_->layer()->GetAnimator());
    left_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    left_window_->SetTransform(left_transform);

    ui::ScopedLayerAnimationSettings divider_widget_settings(
        divider_widget_->GetNativeWindow()->layer()->GetAnimator());
    divider_widget_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    divider_widget_->GetNativeWindow()->SetTransform(divider_transform);

    ui::ScopedLayerAnimationSettings right_settings(
        right_window_->layer()->GetAnimator());
    right_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    right_settings.AddObserver(new ui::ClosureAnimationObserver(
        base::Bind(&SplitViewController::OnAnimationCompleted,
                   weak_factory_.GetWeakPtr())));
    right_window_->SetTransform(right_transform);
  } else {
    left_window_->SetTransform(left_transform);
    divider_widget_->GetNativeWindow()->SetTransform(divider_transform);
    right_window_->SetTransform(right_transform);
  }
}

void SplitViewController::OnAnimationCompleted() {
  // Animation can be cancelled when deactivated.
  if (left_window_ == NULL)
    return;
  UpdateLayout(false);

  for (size_t i = 0; i < to_hide_.size(); ++i)
    to_hide_[i]->Hide();
  to_hide_.clear();

  if (state_ == INACTIVE) {
    left_window_ = NULL;
    right_window_ = NULL;
  }
}

int SplitViewController::GetDefaultDividerPosition() {
  return container_->GetBoundsInScreen().width() / 2;
}

///////////////////////////////////////////////////////////////////////////////
// BezelController::ScrollDelegate:

void SplitViewController::BezelScrollBegin(BezelController::Bezel bezel,
                                           float delta) {
  if (!BezelCanScroll())
    return;

  SetState(SCROLLING);

  const aura::Window::Windows& windows = window_list_provider_->GetWindowList();
  CHECK(windows.size() >= 2);
  aura::Window::Windows::const_reverse_iterator iter = windows.rbegin();
  aura::Window* current_window = *(iter);

  if (delta > 0) {
    right_window_ = current_window;
    left_window_ = *(iter + 1);
  } else {
    left_window_ = current_window;
    right_window_ = *(iter + 1);
  }

  CHECK(left_window_);
  CHECK(right_window_);

  // Calculate divider_scroll_start_position_
  gfx::Screen* screen = gfx::Screen::GetScreenFor(container_);
  const gfx::Rect& display_bounds =
      screen->GetDisplayNearestWindow(container_).bounds();
  gfx::Rect container_bounds = container_->GetBoundsInScreen();
  divider_scroll_start_position_ =
      delta > 0 ? display_bounds.x() - container_bounds.x()
                : display_bounds.right() - container_bounds.x();

  divider_position_ = divider_scroll_start_position_ + delta;
  UpdateLayout(false);
}

void SplitViewController::BezelScrollEnd() {
  if (state_ != SCROLLING)
    return;

  // Max distance from the scroll end position to the middle of the screen where
  // we would go into the split view mode.
  const int kMaxDistanceFromMiddle = 120;
  const int default_divider_position = GetDefaultDividerPosition();
  if (std::abs(default_divider_position - divider_position_) <=
      kMaxDistanceFromMiddle) {
    divider_position_ = default_divider_position;
    SetState(ACTIVE);
  } else if (divider_position_ < default_divider_position) {
    divider_position_ = 0;
    SetState(INACTIVE);
    wm::ActivateWindow(right_window_);
  } else {
    divider_position_ = container_->GetBoundsInScreen().width();
    SetState(INACTIVE);
    wm::ActivateWindow(left_window_);
  }
  UpdateLayout(true);
}

void SplitViewController::BezelScrollUpdate(float delta) {
  if (state_ != SCROLLING)
    return;
  divider_position_ = divider_scroll_start_position_ + delta;
  UpdateLayout(false);
}

bool SplitViewController::BezelCanScroll() {
  return CanActivateSplitViewMode();
}

///////////////////////////////////////////////////////////////////////////////
// DragHandleScrollDelegate:

void SplitViewController::HandleScrollBegin(float delta) {
  CHECK(state_ == ACTIVE);
  state_ = SCROLLING;
  divider_scroll_start_position_ = GetDefaultDividerPosition();
  divider_position_ = divider_scroll_start_position_ + delta;
  UpdateLayout(false);
}

void SplitViewController::HandleScrollEnd() {
  BezelScrollEnd();
}

void SplitViewController::HandleScrollUpdate(float delta) {
  BezelScrollUpdate(delta);
}

///////////////////////////////////////////////////////////////////////////////
// WindowManagerObserver:

void SplitViewController::OnOverviewModeEnter() {
  if (divider_widget_)
    HideDivider();
}

void SplitViewController::OnOverviewModeExit() {
  if (state_ != INACTIVE)
    ShowDivider();
}

void SplitViewController::OnSplitViewModeEnter() {
}

void SplitViewController::OnSplitViewModeExit() {
}

}  // namespace athena
