// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_overview_overlay.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/root_window_finder.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display_observer.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The amount of round applied to the corners of the highlight views.
constexpr int kHighlightScreenRoundRectRadiusDp = 4;

// Creates the widget responsible for displaying the indicators.
std::unique_ptr<views::Widget> CreateWidget() {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.keep_on_top = false;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = false;
  params.parent = Shell::GetContainer(Shell::Get()->GetPrimaryRootWindow(),
                                      kShellWindowId_OverlayContainer);
  widget->set_focus_on_creation(false);
  widget->Init(params);
  return widget;
}

// Computes the transform which rotates the labels |angle| degrees. The point
// of rotation is the relative center point of |bounds|.
gfx::Transform ComputeRotateAroundCenterTransform(const gfx::Rect& bounds,
                                                  double angle) {
  gfx::Transform transform;
  const gfx::Vector2dF center_point_vector =
      bounds.CenterPoint() - bounds.origin();
  transform.Translate(center_point_vector);
  transform.Rotate(angle);
  transform.Translate(-center_point_vector);
  return transform;
}

bool IsPhantomState(IndicatorState indicator_state) {
  return indicator_state == IndicatorState::kPhantomLeft ||
         indicator_state == IndicatorState::kPhantomRight;
}

// Calculate whether the phantom window should physically be on the left or top
// of the screen. kPhantomLeft and kPhantomRight correspond with LEFT_SNAPPED
// and RIGHT_SNAPPED which do not always correspond to the physical left and
// right of the screen. See split_view_controller.h for more details.
bool IsPhantomOnLeftTopOfScreen(IndicatorState indicator_state) {
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  return (indicator_state == IndicatorState::kPhantomLeft &&
          split_view_controller->IsCurrentScreenOrientationPrimary()) ||
         (indicator_state == IndicatorState::kPhantomRight &&
          !split_view_controller->IsCurrentScreenOrientationPrimary());
}

}  // namespace

// View which contains a label and can be rotated. Used by and rotated by
// SplitViewOverviewOverlayView.
class SplitViewOverviewOverlay::RotatedImageLabelView : public RoundedRectView {
 public:
  RotatedImageLabelView()
      : RoundedRectView(kSplitviewLabelRoundRectRadiusDp,
                        kSplitviewLabelBackgroundColor) {
    label_ = new views::Label(base::string16(), views::style::CONTEXT_LABEL);
    label_->SetPaintToLayer();
    label_->layer()->SetFillsBoundsOpaquely(false);
    label_->SetEnabledColor(kSplitviewLabelEnabledColor);
    label_->SetBackgroundColor(kSplitviewLabelBackgroundColor);

    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical,
        gfx::Insets(kSplitviewLabelVerticalInsetDp,
                    kSplitviewLabelHorizontalInsetDp)));
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    AddChildView(label_);
  }

  ~RotatedImageLabelView() override = default;

  void SetLabelText(const base::string16& text) { label_->SetText(text); }

  // Called when the view's bounds are altered. Rotates the view by |angle|
  // degrees.
  void OnBoundsUpdated(const gfx::Rect& bounds, double angle) {
    SetBoundsRect(bounds);
    SetTransform(ComputeRotateAroundCenterTransform(bounds, angle));
  }

 private:
  views::Label* label_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RotatedImageLabelView);
};

// View which contains two highlights on each side indicator where a user should
// drag a selected window in order to initiate splitview. Each highlight has a
// label with instructions to further guide users. The highlights are on the
// left and right of the display in landscape mode, and on the top and bottom of
// the display in landscape mode. The highlights can expand and shrink if a
// window has entered a snap region to display the bounds of the window, if it
// were to get snapped.
class SplitViewOverviewOverlay::SplitViewOverviewOverlayView
    : public views::View,
      public ui::ImplicitAnimationObserver {
 public:
  SplitViewOverviewOverlayView() {
    left_hightlight_view_ =
        new RoundedRectView(kHighlightScreenRoundRectRadiusDp, SK_ColorWHITE);
    right_hightlight_view_ =
        new RoundedRectView(kHighlightScreenRoundRectRadiusDp, SK_ColorWHITE);

    left_hightlight_view_->SetPaintToLayer();
    right_hightlight_view_->SetPaintToLayer();
    left_hightlight_view_->layer()->SetFillsBoundsOpaquely(false);
    right_hightlight_view_->layer()->SetFillsBoundsOpaquely(false);

    AddChildView(left_hightlight_view_);
    AddChildView(right_hightlight_view_);

    left_rotated_view_ = new RotatedImageLabelView();
    right_rotated_view_ = new RotatedImageLabelView();

    AddChildView(left_rotated_view_);
    AddChildView(right_rotated_view_);

    // Nothing is shown initially.
    left_hightlight_view_->layer()->SetOpacity(0.f);
    right_hightlight_view_->layer()->SetOpacity(0.f);
    left_rotated_view_->layer()->SetOpacity(0.f);
    right_rotated_view_->layer()->SetOpacity(0.f);
  }

  ~SplitViewOverviewOverlayView() override {}

  // Called by parent widget when the state machine changes. Handles setting the
  // opacity of the highlights and labels based on the state.
  void OnIndicatorTypeChanged(IndicatorState indicator_state) {
    DCHECK_NE(indicator_state_, indicator_state);

    previous_indicator_state_ = indicator_state_;
    indicator_state_ = indicator_state;

    switch (indicator_state) {
      case IndicatorState::kNone:
        DoSplitviewOpacityAnimation(left_hightlight_view_->layer(),
                                    SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT);
        DoSplitviewOpacityAnimation(right_hightlight_view_->layer(),
                                    SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT);
        DoSplitviewOpacityAnimation(left_rotated_view_->layer(),
                                    SPLITVIEW_ANIMATION_TEXT_FADE_OUT);
        DoSplitviewOpacityAnimation(right_rotated_view_->layer(),
                                    SPLITVIEW_ANIMATION_TEXT_FADE_OUT);
        return;
      case IndicatorState::kDragArea:
      case IndicatorState::kCannotSnap: {
        const bool show = Shell::Get()->split_view_controller()->state() ==
                          SplitViewController::NO_SNAP;

        for (RotatedImageLabelView* view : GetTextViews()) {
          view->SetLabelText(l10n_util::GetStringUTF16(
              indicator_state == IndicatorState::kCannotSnap
                  ? IDS_ASH_SPLIT_VIEW_CANNOT_SNAP
                  : IDS_ASH_SPLIT_VIEW_GUIDANCE));
          DoSplitviewOpacityAnimation(view->layer(),
                                      show ? SPLITVIEW_ANIMATION_TEXT_FADE_IN
                                           : SPLITVIEW_ANIMATION_TEXT_FADE_OUT);
        }

        for (RoundedRectView* view : GetHighlightViews()) {
          view->SetBackgroundColor(
              indicator_state == IndicatorState::kCannotSnap ? SK_ColorBLACK
                                                             : SK_ColorWHITE);
          DoSplitviewOpacityAnimation(
              view->layer(), show ? SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN
                                  : SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT);
        }

        Layout();
        return;
      }
      case IndicatorState::kPhantomLeft:
      case IndicatorState::kPhantomRight: {
        left_rotated_view_->layer()->SetOpacity(0.f);
        right_rotated_view_->layer()->SetOpacity(0.f);

        if (IsPhantomOnLeftTopOfScreen(indicator_state_)) {
          DoSplitviewOpacityAnimation(left_hightlight_view_->layer(),
                                      SPLITVIEW_ANIMATION_PHANTOM_FADE_IN);
          DoSplitviewOpacityAnimation(
              right_hightlight_view_->layer(),
              SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_OUT);
        } else {
          DoSplitviewOpacityAnimation(
              left_hightlight_view_->layer(),
              SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_OUT);
          DoSplitviewOpacityAnimation(right_hightlight_view_->layer(),
                                      SPLITVIEW_ANIMATION_PHANTOM_FADE_IN);
        }
        Layout();
        return;
      }
    }

    NOTREACHED();
  }

  views::View* GetViewForIndicatorType(IndicatorType type) {
    switch (type) {
      case IndicatorType::kLeftHighlight:
        return left_hightlight_view_;
      case IndicatorType::kLeftText:
        return left_rotated_view_;
      case IndicatorType::kRightHighlight:
        return right_hightlight_view_;
      case IndicatorType::kRightText:
        return right_rotated_view_;
    }

    NOTREACHED();
    return nullptr;
  }

  // views::View:
  void Layout() override {
    const bool landscape = Shell::Get()
                               ->split_view_controller()
                               ->IsCurrentScreenOrientationLandscape();

    // Calculate the bounds of the two highlight regions.
    const int highlight_width =
        landscape ? width() * kHighlightScreenPrimaryAxisRatio
                  : width() - 2 * kHighlightScreenEdgePaddingDp;
    const int highlight_height =
        landscape ? height() - 2 * kHighlightScreenEdgePaddingDp
                  : height() * kHighlightScreenPrimaryAxisRatio;

    // The origin of the right highlight view in landscape, or the bottom
    // highlight view in portrait.
    const gfx::Point right_bottom_origin(
        landscape ? width() - highlight_width - kHighlightScreenEdgePaddingDp
                  : kHighlightScreenEdgePaddingDp,
        landscape
            ? kHighlightScreenEdgePaddingDp
            : height() - highlight_height - kHighlightScreenEdgePaddingDp);

    // Apply a transform to the left and right highlights if one is to expand to
    // show a phantom window. The expanding window will be transformed by
    // |main_transform|. The other window, which will shrink and fade out, will
    // be transformed by |other_transform|.
    gfx::Transform main_transform, other_transform;
    const bool phantom_left =
        IsPhantomOnLeftTopOfScreen(indicator_state_) ||
        IsPhantomOnLeftTopOfScreen(previous_indicator_state_);
    if (IsPhantomState(indicator_state_) ||
        (indicator_state_ == IndicatorState::kDragArea &&
         IsPhantomState(previous_indicator_state_))) {
      // Get the phantom window bounds from the split view controller.
      gfx::Rect phantom_bounds =
          Shell::Get()->split_view_controller()->GetSnappedWindowBoundsInScreen(
              GetWidget()->GetNativeWindow(), phantom_left
                                                  ? SplitViewController::LEFT
                                                  : SplitViewController::RIGHT);
      phantom_bounds.Inset(kHighlightScreenEdgePaddingDp,
                           kHighlightScreenEdgePaddingDp);

      // Compute both |main_transform| and |other_transform|. In landscape mode
      // use x and width values. In portrait mode use y and height values.
      if (landscape) {
        if (!phantom_left) {
          // |main_transform| corresponds to the right window, which changes x
          // position as well as scale, so apply a translation.
          main_transform.Translate(gfx::Vector2dF(
              -(right_bottom_origin.x() - width() +
                kHighlightScreenEdgePaddingDp + phantom_bounds.width()),
              0.f));
        }
        // Apply a scale to scale the width to the width of |phantom_bounds|.
        main_transform.Scale(static_cast<double>(phantom_bounds.width()) /
                                 static_cast<double>(highlight_width),
                             1.0);

        if (phantom_left) {
          // |other_transform| corresponds to the right window, which changes x
          // position as well as scale, so apply a translation.
          other_transform.Translate(gfx::Vector2dF(highlight_width, 0.f));
        }
        // Scale the other window so that it becomes hidden.
        other_transform.Scale(1.0 / static_cast<double>(highlight_width), 1.0);
      } else {
        if (!phantom_left) {
          // |main_transform| corresponds to the bottom window, which changes y
          // position as well as scale, so apply a translation.
          main_transform.Translate(gfx::Vector2dF(
              0.f, -(right_bottom_origin.y() - height() +
                     kHighlightScreenEdgePaddingDp + phantom_bounds.height())));
        }
        // Apply a scale to scale the height to the height of |phantom_bounds|.
        main_transform.Scale(1.0, static_cast<double>(phantom_bounds.height()) /
                                      static_cast<double>(highlight_height));

        if (phantom_left) {
          // |other_transform| corresponds to the bottom window, which changes y
          // position as well as scale, so apply a translation.
          other_transform.Translate(gfx::Vector2dF(0.f, highlight_height));
        }
        // Scale the other window so that it becomes hidden.
        other_transform.Scale(1.0, 1.0 / static_cast<double>(highlight_height));
      }

      if (IsPhantomState(previous_indicator_state_)) {
        // If the previous state was a phantom state, first apply a transform,
        // otherwise no animation will happen if we try to transform from
        // identity to identity. (OnImplicitAnimationsCompleted sets the bounds
        // and all transforms to identity to preserve rounded edges).
        left_hightlight_view_->layer()->SetTransform(
            phantom_left ? main_transform : other_transform);
        right_hightlight_view_->layer()->SetTransform(
            phantom_left ? other_transform : main_transform);
        main_transform.MakeIdentity();
        other_transform.MakeIdentity();
      }
    }

    DoSplitviewTransformAnimation(
        left_hightlight_view_->layer(),
        SPLITVIEW_ANIMATION_PHANTOM_SLIDE_IN_OUT,
        phantom_left ? main_transform : other_transform, this);
    left_hightlight_bounds_ =
        gfx::Rect(kHighlightScreenEdgePaddingDp, kHighlightScreenEdgePaddingDp,
                  highlight_width, highlight_height);
    left_hightlight_view_->SetBoundsRect(left_hightlight_bounds_);

    DoSplitviewTransformAnimation(
        right_hightlight_view_->layer(),
        SPLITVIEW_ANIMATION_PHANTOM_SLIDE_IN_OUT,
        phantom_left ? other_transform : main_transform, nullptr);
    right_hightlight_bounds_ =
        gfx::Rect(right_bottom_origin.x(), right_bottom_origin.y(),
                  highlight_width, highlight_height);
    right_hightlight_view_->SetBoundsRect(right_hightlight_bounds_);

    // Calculate the bounds of the views which contain the guidance text and
    // icon. Rotate the two views in landscape mode.
    const gfx::Size size(
        left_rotated_view_->GetPreferredSize().width(),
        std::max(kSplitviewLabelPreferredHeightDp,
                 left_rotated_view_->GetPreferredSize().height()));
    gfx::Rect left_rotated_bounds(highlight_width / 2 - size.width() / 2,
                                  highlight_height / 2 - size.height() / 2,
                                  size.width(), size.height());
    gfx::Rect right_rotated_bounds = left_rotated_bounds;
    right_rotated_bounds.Offset(right_bottom_origin.x(),
                                right_bottom_origin.y());

    // In portrait mode, there is no need to rotate the text and warning icon.
    // In landscape mode, rotate the left text 90 degrees clockwise in rtl and
    // 90 degress anti clockwise in ltr. The right text is rotated 90 degrees in
    // the opposite direction of the left text.
    double left_rotation_angle = 0.0;
    if (landscape)
      left_rotation_angle = 90.0 * (base::i18n::IsRTL() ? 1 : -1);

    left_rotated_view_->OnBoundsUpdated(left_rotated_bounds,
                                        left_rotation_angle /* angle */);
    right_rotated_view_->OnBoundsUpdated(right_rotated_bounds,
                                         -left_rotation_angle /* angle */);
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    // Set the final bounds and the layer transforms to identity, so that the
    // proper rounding on the rounded rect corners show up.
    gfx::Rect phantom_bounds =
        Shell::Get()->split_view_controller()->GetSnappedWindowBoundsInScreen(
            GetWidget()->GetNativeWindow(),
            indicator_state_ == IndicatorState::kPhantomLeft
                ? SplitViewController::LEFT
                : SplitViewController::RIGHT);
    phantom_bounds.Inset(kHighlightScreenEdgePaddingDp,
                         kHighlightScreenEdgePaddingDp);

    constexpr gfx::Rect kEmptyRect;
    left_hightlight_view_->layer()->SetTransform(gfx::Transform());
    right_hightlight_view_->layer()->SetTransform(gfx::Transform());
    if (IsPhantomOnLeftTopOfScreen(indicator_state_)) {
      left_hightlight_view_->SetBoundsRect(phantom_bounds);
      right_hightlight_view_->SetBoundsRect(kEmptyRect);
    } else if (IsPhantomState(indicator_state_)) {
      left_hightlight_view_->SetBoundsRect(kEmptyRect);
      right_hightlight_view_->SetBoundsRect(phantom_bounds);
    } else {
      left_hightlight_view_->SetBoundsRect(left_hightlight_bounds_);
      right_hightlight_view_->SetBoundsRect(right_hightlight_bounds_);
    }
  }

 private:
  std::vector<RoundedRectView*> GetHighlightViews() {
    return {left_hightlight_view_, right_hightlight_view_};
  }
  std::vector<RotatedImageLabelView*> GetTextViews() {
    return {left_rotated_view_, right_rotated_view_};
  }

  RoundedRectView* left_hightlight_view_ = nullptr;
  RoundedRectView* right_hightlight_view_ = nullptr;
  RotatedImageLabelView* left_rotated_view_ = nullptr;
  RotatedImageLabelView* right_rotated_view_ = nullptr;

  // Cache the bounds calculated in Layout(), so that they do not have to be
  // recalculated when animation is finished.
  gfx::Rect left_hightlight_bounds_;
  gfx::Rect right_hightlight_bounds_;

  IndicatorState indicator_state_ = IndicatorState::kNone;
  IndicatorState previous_indicator_state_ = IndicatorState::kNone;

  DISALLOW_COPY_AND_ASSIGN(SplitViewOverviewOverlayView);
};

SplitViewOverviewOverlay::SplitViewOverviewOverlay() {
  overlay_view_ = new SplitViewOverviewOverlayView();
  widget_ = CreateWidget();
  widget_->SetContentsView(overlay_view_);
  widget_->Show();
}

SplitViewOverviewOverlay::~SplitViewOverviewOverlay() = default;

void SplitViewOverviewOverlay::SetIndicatorState(
    IndicatorState indicator_state,
    const gfx::Point& event_location) {
  if (indicator_state == current_indicator_state_)
    return;

  // Reparent the widget if needed.
  aura::Window* target = ash::wm::GetRootWindowAt(event_location);
  aura::Window* root_window = target->GetRootWindow();
  if (widget_->GetNativeView()->GetRootWindow() != root_window) {
    views::Widget::ReparentNativeView(
        widget_->GetNativeView(),
        Shell::GetContainer(root_window, kShellWindowId_OverlayContainer));
    widget_->SetContentsView(overlay_view_);
  }
  gfx::Rect bounds = screen_util::GetDisplayWorkAreaBoundsInParent(
      root_window->GetChildById(kShellWindowId_OverlayContainer));
  ::wm::ConvertRectToScreen(root_window, &bounds);
  widget_->SetBounds(bounds);

  current_indicator_state_ = indicator_state;
  overlay_view_->OnIndicatorTypeChanged(current_indicator_state_);
}

void SplitViewOverviewOverlay::OnDisplayBoundsChanged() {
  overlay_view_->Layout();
}

bool SplitViewOverviewOverlay::GetIndicatorTypeVisibilityForTesting(
    IndicatorType type) const {
  return overlay_view_->GetViewForIndicatorType(type)->layer()->opacity() > 0.f;
}

}  // namespace ash
