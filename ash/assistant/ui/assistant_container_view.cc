// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_container_view.h"

#include <algorithm>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_main_view.h"
#include "ash/assistant/ui/assistant_mini_view.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_web_view.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

// Appearance.
constexpr SkColor kBackgroundColor = SK_ColorWHITE;
constexpr int kMiniUiCornerRadiusDip = 24;
constexpr int kMarginBottomDip = 8;

// Animation.
constexpr int kResizeAnimationDurationMs = 250;

// Window properties.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kOnlyAllowMouseClickEvents, false);

// AssistantContainerEventTargeter ---------------------------------------------

class AssistantContainerEventTargeter : public aura::WindowTargeter {
 public:
  AssistantContainerEventTargeter() = default;
  ~AssistantContainerEventTargeter() override = default;

  // aura::WindowTargeter:
  bool SubtreeShouldBeExploredForEvent(aura::Window* window,
                                       const ui::LocatedEvent& event) override {
    if (window->GetProperty(kOnlyAllowMouseClickEvents)) {
      if (event.type() != ui::ET_MOUSE_PRESSED &&
          event.type() != ui::ET_MOUSE_RELEASED) {
        return false;
      }
    }
    return aura::WindowTargeter::SubtreeShouldBeExploredForEvent(window, event);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantContainerEventTargeter);
};

// AssistantContainerLayout ----------------------------------------------------

// The AssistantContainerLayout calculates preferred size to fit the largest
// visible child. Children that are not visible are not factored in. During
// layout, children are horizontally centered and bottom aligned.
class AssistantContainerLayout : public views::LayoutManager {
 public:
  AssistantContainerLayout() = default;
  ~AssistantContainerLayout() override = default;

  // views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* host) const override {
    int preferred_width = 0;

    for (int i = 0; i < host->child_count(); ++i) {
      const views::View* child = host->child_at(i);

      // We do not include invisible children in our size calculation.
      if (!child->visible())
        continue;

      // Our preferred width is the width of our largest visible child.
      preferred_width =
          std::max(child->GetPreferredSize().width(), preferred_width);
    }

    return gfx::Size(preferred_width,
                     GetPreferredHeightForWidth(host, preferred_width));
  }

  int GetPreferredHeightForWidth(const views::View* host,
                                 int width) const override {
    int preferred_height = 0;

    for (int i = 0; i < host->child_count(); ++i) {
      const views::View* child = host->child_at(i);

      // We do not include invisible children in our size calculation.
      if (!child->visible())
        continue;

      // Our preferred height is the height of our largest visible child.
      preferred_height =
          std::max(child->GetHeightForWidth(width), preferred_height);
    }

    return preferred_height;
  }

  void Layout(views::View* host) override {
    const int host_center_x = host->GetBoundsInScreen().CenterPoint().x();
    const int host_height = host->height();

    for (int i = 0; i < host->child_count(); ++i) {
      views::View* child = host->child_at(i);

      const gfx::Size child_size = child->GetPreferredSize();

      // Children are horizontally centered. This means that both the |host|
      // and child views share the same center x-coordinate relative to the
      // screen. We use this center value when placing our children because
      // deriving center from the host width causes rounding inconsistencies
      // that are especially noticeable during animation.
      gfx::Point child_center(host_center_x, /*y=*/0);
      views::View::ConvertPointFromScreen(host, &child_center);
      int child_left = child_center.x() - child_size.width() / 2;

      // Children are bottom aligned.
      int child_top = host_height - child_size.height();

      child->SetBounds(child_left, child_top, child_size.width(),
                       child_size.height());
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantContainerLayout);
};

}  // namespace

// AssistantContainerView ------------------------------------------------------

AssistantContainerView::AssistantContainerView(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  set_accept_events(true);
  SetAnchor(nullptr);
  set_close_on_deactivate(false);
  set_color(kBackgroundColor);
  set_margins(gfx::Insets());
  set_shadow(views::BubbleBorder::Shadow::NO_ASSETS);
  set_title_margins(gfx::Insets());

  views::BubbleDialogDelegateView::CreateBubble(this);

  // These attributes can only be set after bubble creation:
  GetBubbleFrameView()->bubble_border()->SetCornerRadius(kCornerRadiusDip);

  // Update the initial shadow layer with correct corner radius.
  UpdateShadow();

  // Add the shadow layer on top of the non-client view layer.
  shadow_layer_.SetFillsBoundsOpaquely(false);
  GetBubbleFrameView()->SetPaintToLayer();
  GetBubbleFrameView()->layer()->SetFillsBoundsOpaquely(false);
  GetBubbleFrameView()->layer()->Add(&shadow_layer_);

  // The AssistantController owns the view hierarchy to which
  // AssistantContainerView belongs so is guaranteed to outlive it.
  assistant_controller_->ui_controller()->AddModelObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
  keyboard::KeyboardController::Get()->AddObserver(this);
}

AssistantContainerView::~AssistantContainerView() {
  keyboard::KeyboardController::Get()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
  assistant_controller_->ui_controller()->RemoveModelObserver(this);
}

// static
void AssistantContainerView::OnlyAllowMouseClickEvents(aura::Window* window) {
  window->SetProperty(kOnlyAllowMouseClickEvents, true);
}

void AssistantContainerView::AddedToWidget() {
  GetWidget()->GetNativeWindow()->SetEventTargeter(
      std::make_unique<AssistantContainerEventTargeter>());
}

void AssistantContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantContainerView::PreferredSizeChanged() {
  if (!GetWidget())
    return;

  const bool visible =
      assistant_controller_->ui_controller()->model()->visibility() ==
      AssistantVisibility::kVisible;

  // Calculate the target radius value with or without animation.
  radius_end_ = assistant_controller_->ui_controller()->model()->ui_mode() ==
                        AssistantUiMode::kMiniUi
                    ? kMiniUiCornerRadiusDip
                    : kCornerRadiusDip;

  // When visible, size changes are animated.
  if (visible) {
    resize_animation_ = std::make_unique<gfx::SlideAnimation>(this);
    resize_animation_->SetSlideDuration(kResizeAnimationDurationMs);

    // Cache start and end animation values.
    resize_start_ = gfx::SizeF(size());
    resize_end_ = gfx::SizeF(GetPreferredSize());

    radius_start_ =
        GetBubbleFrameView()->bubble_border()->GetBorderCornerRadius();

    // Start animation.
    resize_animation_->Show();
    return;
  }

  // Clear any existing animation.
  resize_animation_.reset();

  // Update corner radius value without animation.
  GetBubbleFrameView()->bubble_border()->SetCornerRadius(radius_end_);

  SizeToContents();
}

int AssistantContainerView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void AssistantContainerView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  params->context = Shell::Get()->GetRootWindowForNewWindows();
  params->corner_radius = kCornerRadiusDip;
  params->keep_on_top = true;
}

void AssistantContainerView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Update the shadow layer when resizing the window.
  UpdateShadow();

  SchedulePaint();
}

void AssistantContainerView::Init() {
  SetLayoutManager(std::make_unique<AssistantContainerLayout>());

  // We need to paint to our own layer so we can clip child layers.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  // Main view.
  assistant_main_view_ = new AssistantMainView(assistant_controller_);
  AddChildView(assistant_main_view_);

  // Mini view.
  assistant_mini_view_ = new AssistantMiniView(assistant_controller_);
  assistant_mini_view_->set_delegate(assistant_controller_->ui_controller());
  AddChildView(assistant_mini_view_);

  // Web view.
  assistant_web_view_ = new AssistantWebView(assistant_controller_);
  AddChildView(assistant_web_view_);

  // Update the view state based on the current UI mode.
  OnUiModeChanged(assistant_controller_->ui_controller()->model()->ui_mode());
}

void AssistantContainerView::RequestFocus() {
  if (!GetWidget() || !GetWidget()->IsActive())
    return;

  switch (assistant_controller_->ui_controller()->model()->ui_mode()) {
    case AssistantUiMode::kMiniUi:
      if (assistant_mini_view_)
        assistant_mini_view_->RequestFocus();
      break;
    case AssistantUiMode::kMainUi:
      if (assistant_main_view_)
        assistant_main_view_->RequestFocus();
      break;
    case AssistantUiMode::kWebUi:
      if (assistant_web_view_)
        assistant_web_view_->RequestFocus();
      break;
  }
}

void AssistantContainerView::SetAnchor(aura::Window* root_window) {
  // If |root_window| is not specified, we'll use the root window corresponding
  // to where new windows will be opened.
  if (!root_window)
    root_window = Shell::Get()->GetRootWindowForNewWindows();

  // Anchor to the display matching |root_window|.
  display::Display display = display::Screen::GetScreen()->GetDisplayMatching(
      root_window->GetBoundsInScreen());

  // Align to the bottom, horizontal center of the work area.
  gfx::Rect work_area = display.work_area();
  gfx::Rect anchor =
      gfx::Rect(work_area.x(), work_area.bottom() - kMarginBottomDip,
                work_area.width(), 0);

  SetAnchorRect(anchor);
  set_arrow(views::BubbleBorder::Arrow::BOTTOM_CENTER);
}

void AssistantContainerView::OnUiModeChanged(AssistantUiMode ui_mode) {
  for (int i = 0; i < child_count(); ++i) {
    child_at(i)->SetVisible(false);
  }

  switch (ui_mode) {
    case AssistantUiMode::kMiniUi:
      assistant_mini_view_->SetVisible(true);
      break;
    case AssistantUiMode::kMainUi:
      assistant_main_view_->SetVisible(true);
      break;
    case AssistantUiMode::kWebUi:
      assistant_web_view_->SetVisible(true);
      break;
  }

  PreferredSizeChanged();
  RequestFocus();
}

// TODO(dmblack): Improve performance of this animation using transformations
// for GPU acceleration. Lower spec hardware may struggle with numerous layouts.
void AssistantContainerView::AnimationProgressed(
    const gfx::Animation* animation) {
  if (!GetWidget())
    return;

  // Retrieve current bounds.
  gfx::Rect bounds = GetWidget()->GetWindowBoundsInScreen();

  // Our view is horizontally centered and bottom aligned. As such, we should
  // retain the same |center_x| and |bottom| positions after resizing.
  const int bottom = bounds.bottom();
  const int center_x = bounds.CenterPoint().x();

  // Interpolate size at our current animation value.
  const gfx::SizeF size = gfx::Tween::SizeValueBetween(
      animation->GetCurrentValue(), resize_start_, resize_end_);

  // Use our interpolated size.
  bounds.set_size(gfx::Size(size.width(), size.height()));

  // Maintain original |center_x| and |bottom| positions.
  bounds.set_x(center_x - (bounds.width() / 2));
  bounds.set_y(bottom - bounds.height());

  // Interpolate the correct radius.
  const int corner_radius = gfx::Tween::LinearIntValueBetween(
      animation->GetCurrentValue(), radius_start_, radius_end_);

  // Clip round corners on child views by directly changing the non-client
  // view corner radius of this bubble widget.
  GetBubbleFrameView()->bubble_border()->SetCornerRadius(corner_radius);

  GetWidget()->SetBounds(bounds);
}

void AssistantContainerView::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  if (root_window == Shell::Get()->GetRootWindowForDisplayId(display.id()))
    SetAnchor(root_window);
}

void AssistantContainerView::OnKeyboardWorkspaceDisplacingBoundsChanged(
    const gfx::Rect& new_bounds) {
  SetAnchor(GetWidget()->GetNativeWindow()->GetRootWindow());
}

void AssistantContainerView::UpdateShadow() {
  // Initialize shadow parameters in painting.
  gfx::ShadowValues shadow_values =
      gfx::ShadowValue::MakeMdShadowValues(wm::kShadowElevationActiveWindow);

  const int corner_radius =
      GetBubbleFrameView()->bubble_border()->GetBorderCornerRadius();
  border_shadow_delegate_ = std::make_unique<views::BorderShadowLayerDelegate>(
      shadow_values, GetLocalBounds(), SK_ColorWHITE, corner_radius);

  shadow_layer_.set_delegate(border_shadow_delegate_.get());
  shadow_layer_.SetBounds(
      gfx::ToEnclosingRect(border_shadow_delegate_->GetPaintedBounds()));
}

}  // namespace ash
