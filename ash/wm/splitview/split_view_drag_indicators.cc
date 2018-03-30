// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_drag_indicators.h"

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

// When animating, this is the location of the split view label as a ratio of
// the width or height.
constexpr double kSplitviewLabelExpandTranslationPrimaryAxisRatio = 0.20;
constexpr double kSplitviewLabelShrinkTranslationPrimaryAxisRatio = 0.05;

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

bool IsPreviewAreaState(IndicatorState indicator_state) {
  return indicator_state == IndicatorState::kPreviewAreaLeft ||
         indicator_state == IndicatorState::kPreviewAreaRight;
}

// Calculate whether the  preview area should physically be on the left or top
// of the screen. kPreviewAreaLeft and kPreviewAreaRight correspond with
// LEFT_SNAPPED and RIGHT_SNAPPED which do not always correspond to the physical
// left and right of the screen. See split_view_controller.h for more details.
bool IsPreviewAreaOnLeftTopOfScreen(IndicatorState indicator_state) {
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  return (indicator_state == IndicatorState::kPreviewAreaLeft &&
          split_view_controller->IsCurrentScreenOrientationPrimary()) ||
         (indicator_state == IndicatorState::kPreviewAreaRight &&
          !split_view_controller->IsCurrentScreenOrientationPrimary());
}

// Helper function to compute the transform for the indicator labels when the
// view changes states. |main_transform| determines what ratio of the highlight
// we want to shift to. |non_transformed_bounds| represents the bounds of the
// label before its transform is applied; the centerpoint is used to calculate
// the amount of shift. One of |highlight_width| or |highlight_height| will be
// used to calculate the amount of shift as well, depending on |landscape|. If
// the label is not |left_or_top| (right or bottom) we will translate in the
// other direction.
gfx::Transform ComputeLabelTransform(bool main_transform,
                                     const gfx::Rect& non_transformed_bounds,
                                     int highlight_width,
                                     int highlight_height,
                                     bool landscape,
                                     bool left_or_top) {
  // Compute the distance of the translation.
  const float ratio = main_transform
                          ? kSplitviewLabelExpandTranslationPrimaryAxisRatio
                          : kSplitviewLabelShrinkTranslationPrimaryAxisRatio;
  const gfx::Point center_point = non_transformed_bounds.CenterPoint();
  const int primary_axis_center =
      landscape ? center_point.x() : center_point.y();
  const int highlight_length = landscape ? highlight_width : highlight_height;
  const float translate =
      std::fabs(ratio * highlight_length - primary_axis_center);

  // Translate along x for landscape, along y for portrait.
  gfx::Vector2dF translation(landscape ? translate : 0,
                             landscape ? 0 : translate);
  // Translate in other direction if right or bottom label.
  if (!left_or_top)
    translation = -translation;
  gfx::Transform transform;
  transform.Translate(translation);
  return transform;
}

}  // namespace

// View which contains a label and can be rotated. Used by and rotated by
// SplitViewDragIndicatorsView.
class SplitViewDragIndicators::RotatedImageLabelView : public views::View {
 public:
  RotatedImageLabelView() {
    label_ = new views::Label(base::string16(), views::style::CONTEXT_LABEL);
    label_->SetPaintToLayer();
    label_->layer()->SetFillsBoundsOpaquely(false);
    label_->SetEnabledColor(kSplitviewLabelEnabledColor);
    label_->SetBackgroundColor(kSplitviewLabelBackgroundColor);

    // Use |label_parent_| to add padding and rounded edges to the text. Create
    // this extra view so that we can rotate the label, while having a slide
    // animation at times on the whole thing.
    label_parent_ = new RoundedRectView(kSplitviewLabelRoundRectRadiusDp,
                                        kSplitviewLabelBackgroundColor);
    label_parent_->SetPaintToLayer();
    label_parent_->layer()->SetFillsBoundsOpaquely(false);
    label_parent_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical,
        gfx::Insets(kSplitviewLabelVerticalInsetDp,
                    kSplitviewLabelHorizontalInsetDp)));
    label_parent_->AddChildView(label_);

    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    AddChildView(label_parent_);
  }

  ~RotatedImageLabelView() override = default;

  void SetLabelText(const base::string16& text) { label_->SetText(text); }

  // Called when the view's bounds are altered. Rotates the view by |angle|
  // degrees.
  void OnBoundsUpdated(const gfx::Rect& bounds, double angle) {
    SetBoundsRect(bounds);
    label_parent_->SetBoundsRect(gfx::Rect(bounds.size()));
    label_parent_->SetTransform(
        ComputeRotateAroundCenterTransform(bounds, angle));
  }

 protected:
  gfx::Size CalculatePreferredSize() const override {
    return label_parent_->GetPreferredSize();
  }

 private:
  RoundedRectView* label_parent_ = nullptr;
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
class SplitViewDragIndicators::SplitViewDragIndicatorsView
    : public views::View,
      public ui::ImplicitAnimationObserver {
 public:
  SplitViewDragIndicatorsView() {
    left_highlight_view_ =
        new RoundedRectView(kHighlightScreenRoundRectRadiusDp, SK_ColorWHITE);
    right_highlight_view_ =
        new RoundedRectView(kHighlightScreenRoundRectRadiusDp, SK_ColorWHITE);

    left_highlight_view_->SetPaintToLayer();
    right_highlight_view_->SetPaintToLayer();
    left_highlight_view_->layer()->SetFillsBoundsOpaquely(false);
    right_highlight_view_->layer()->SetFillsBoundsOpaquely(false);

    AddChildView(left_highlight_view_);
    AddChildView(right_highlight_view_);

    left_rotated_view_ = new RotatedImageLabelView();
    right_rotated_view_ = new RotatedImageLabelView();

    AddChildView(left_rotated_view_);
    AddChildView(right_rotated_view_);

    // Nothing is shown initially.
    left_highlight_view_->layer()->SetOpacity(0.f);
    right_highlight_view_->layer()->SetOpacity(0.f);
    left_rotated_view_->layer()->SetOpacity(0.f);
    right_rotated_view_->layer()->SetOpacity(0.f);
  }

  ~SplitViewDragIndicatorsView() override {}

  // Called by parent widget when the state machine changes. Handles setting the
  // opacity of the highlights and labels based on the state.
  void OnIndicatorTypeChanged(IndicatorState indicator_state) {
    DCHECK_NE(indicator_state_, indicator_state);

    previous_indicator_state_ = indicator_state_;
    indicator_state_ = indicator_state;

    switch (indicator_state) {
      case IndicatorState::kNone:
        DoSplitviewOpacityAnimation(left_highlight_view_->layer(),
                                    SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT);
        DoSplitviewOpacityAnimation(right_highlight_view_->layer(),
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
          SplitviewAnimationType animation_type;
          if (!show) {
            animation_type = SPLITVIEW_ANIMATION_TEXT_FADE_OUT_WITH_HIGHLIGHT;
          } else {
            animation_type =
                IsPreviewAreaState(previous_indicator_state_)
                    ? SPLITVIEW_ANIMATION_TEXT_FADE_IN_WITH_HIGHLIGHT
                    : SPLITVIEW_ANIMATION_TEXT_FADE_IN;
          }
          DoSplitviewOpacityAnimation(view->layer(), animation_type);
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
      case IndicatorState::kPreviewAreaLeft:
      case IndicatorState::kPreviewAreaRight: {
        for (RotatedImageLabelView* view : GetTextViews()) {
          DoSplitviewOpacityAnimation(view->layer(),
                                      SPLITVIEW_ANIMATION_TEXT_FADE_OUT);
        }

        if (IsPreviewAreaOnLeftTopOfScreen(indicator_state_)) {
          DoSplitviewOpacityAnimation(left_highlight_view_->layer(),
                                      SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_IN);
          DoSplitviewOpacityAnimation(
              right_highlight_view_->layer(),
              SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_OUT);
        } else {
          DoSplitviewOpacityAnimation(
              left_highlight_view_->layer(),
              SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_OUT);
          DoSplitviewOpacityAnimation(right_highlight_view_->layer(),
                                      SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_IN);
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
        return left_highlight_view_;
      case IndicatorType::kLeftText:
        return left_rotated_view_;
      case IndicatorType::kRightHighlight:
        return right_highlight_view_;
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
    // show a preview area. The expanding window will be transformed by
    // |main_transform|. The other highlight, which will shrink and fade out,
    // will be transformed by |other_transform|.
    gfx::Transform main_transform, other_transform;
    const bool preview_left =
        IsPreviewAreaOnLeftTopOfScreen(indicator_state_) ||
        IsPreviewAreaOnLeftTopOfScreen(previous_indicator_state_);
    if (IsPreviewAreaState(indicator_state_) ||
        (indicator_state_ == IndicatorState::kDragArea &&
         IsPreviewAreaState(previous_indicator_state_))) {
      // Get the preview area bounds from the split view controller.
      gfx::Rect preview_area_bounds =
          Shell::Get()->split_view_controller()->GetSnappedWindowBoundsInScreen(
              GetWidget()->GetNativeWindow(), preview_left
                                                  ? SplitViewController::LEFT
                                                  : SplitViewController::RIGHT);
      preview_area_bounds.Inset(kHighlightScreenEdgePaddingDp,
                                kHighlightScreenEdgePaddingDp);

      // Compute both |main_transform| and |other_transform|. In landscape mode
      // use x and width values. In portrait mode use y and height values.
      if (landscape) {
        if (!preview_left) {
          // |main_transform| corresponds to the right window, which changes x
          // position as well as scale, so apply a translation.
          main_transform.Translate(gfx::Vector2dF(
              -(right_bottom_origin.x() - width() +
                kHighlightScreenEdgePaddingDp + preview_area_bounds.width()),
              0.f));
        }
        // Apply a scale to scale the width to the width of
        // |preview_area_bounds|.
        main_transform.Scale(static_cast<double>(preview_area_bounds.width()) /
                                 static_cast<double>(highlight_width),
                             1.0);

        if (preview_left) {
          // |other_transform| corresponds to the right window, which changes x
          // position as well as scale, so apply a translation.
          other_transform.Translate(gfx::Vector2dF(highlight_width, 0.f));
        }
        // Scale the other window so that it becomes hidden.
        other_transform.Scale(1.0 / static_cast<double>(highlight_width), 1.0);
      } else {
        if (!preview_left) {
          // |main_transform| corresponds to the bottom window, which changes y
          // position as well as scale, so apply a translation.
          main_transform.Translate(gfx::Vector2dF(
              0.f,
              -(right_bottom_origin.y() - height() +
                kHighlightScreenEdgePaddingDp + preview_area_bounds.height())));
        }
        // Apply a scale to scale the height to the height of
        // |preview_area_bounds|.
        main_transform.Scale(1.0,
                             static_cast<double>(preview_area_bounds.height()) /
                                 static_cast<double>(highlight_height));

        if (preview_left) {
          // |other_transform| corresponds to the bottom window, which changes y
          // position as well as scale, so apply a translation.
          other_transform.Translate(gfx::Vector2dF(0.f, highlight_height));
        }
        // Scale the other window so that it becomes hidden.
        other_transform.Scale(1.0, 1.0 / static_cast<double>(highlight_height));
      }

      if (IsPreviewAreaState(previous_indicator_state_)) {
        // If the previous state was a preview state, first apply a transform,
        // otherwise no animation will happen if we try to transform from
        // identity to identity. (OnImplicitAnimationsCompleted sets the bounds
        // and all transforms to identity to preserve rounded edges).
        left_highlight_view_->layer()->SetTransform(
            preview_left ? main_transform : other_transform);
        right_highlight_view_->layer()->SetTransform(
            preview_left ? other_transform : main_transform);
        main_transform.MakeIdentity();
        other_transform.MakeIdentity();
      }
    }

    left_highlight_bounds_ =
        gfx::Rect(kHighlightScreenEdgePaddingDp, kHighlightScreenEdgePaddingDp,
                  highlight_width, highlight_height);
    right_highlight_bounds_ =
        gfx::Rect(right_bottom_origin.x(), right_bottom_origin.y(),
                  highlight_width, highlight_height);
    if (base::i18n::IsRTL() && landscape) {
      gfx::Rect temp = left_highlight_bounds_;
      left_highlight_bounds_ = right_highlight_bounds_;
      right_highlight_bounds_ = temp;
    }

    DoSplitviewTransformAnimation(
        left_highlight_view_->layer(),
        SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN_OUT,
        preview_left ? main_transform : other_transform, this);
    left_highlight_view_->SetBoundsRect(left_highlight_bounds_);

    DoSplitviewTransformAnimation(
        right_highlight_view_->layer(),
        SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN_OUT,
        preview_left ? other_transform : main_transform, nullptr);
    right_highlight_view_->SetBoundsRect(right_highlight_bounds_);

    // Calculate the bounds of the views which contain the guidance text and
    // icon. Rotate the two views in landscape mode.
    const gfx::Size size(left_rotated_view_->GetPreferredSize().width(),
                         kSplitviewLabelPreferredHeightDp);
    gfx::Rect left_rotated_bounds(highlight_width / 2 - size.width() / 2,
                                  highlight_height / 2 - size.height() / 2,
                                  size.width(), size.height());
    gfx::Rect right_rotated_bounds = left_rotated_bounds;
    left_rotated_bounds.Offset(kHighlightScreenEdgePaddingDp,
                               kHighlightScreenEdgePaddingDp);
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
                                        /*angle=*/left_rotation_angle);
    right_rotated_view_->OnBoundsUpdated(right_rotated_bounds,
                                         /*angle=*/-left_rotation_angle);

    // Compute the transform for the labels. The labels slide in and out when
    // moving between states.
    gfx::Transform main_rotated_transform, other_rotated_transform;
    SplitviewAnimationType animation = SPLITVIEW_ANIMATION_TEXT_SLIDE_IN;
    if (IsPreviewAreaState(indicator_state_)) {
      animation = SPLITVIEW_ANIMATION_TEXT_SLIDE_OUT;
      main_rotated_transform = ComputeLabelTransform(
          preview_left, left_rotated_bounds, highlight_width, highlight_height,
          landscape, preview_left);
      other_rotated_transform = ComputeLabelTransform(
          !preview_left, left_rotated_bounds, highlight_width, highlight_height,
          landscape, preview_left);
    }

    DoSplitviewTransformAnimation(
        left_rotated_view_->layer(), animation,
        preview_left ? main_rotated_transform : other_rotated_transform,
        nullptr);
    DoSplitviewTransformAnimation(
        right_rotated_view_->layer(), animation,
        preview_left ? other_rotated_transform : main_rotated_transform,
        nullptr);
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    // Set the final bounds and the layer transforms to identity, so that the
    // proper rounding on the rounded rect corners show up.
    const bool flip =
        base::i18n::IsRTL() && Shell::Get()
                                   ->split_view_controller()
                                   ->IsCurrentScreenOrientationLandscape();
    const bool left =
        (indicator_state_ == IndicatorState::kPreviewAreaLeft && !flip) ||
        (indicator_state_ == IndicatorState::kPreviewAreaRight && flip);
    gfx::Rect preview_area_bounds =
        Shell::Get()->split_view_controller()->GetSnappedWindowBoundsInScreen(
            GetWidget()->GetNativeWindow(),
            left ? SplitViewController::LEFT : SplitViewController::RIGHT);
    preview_area_bounds.Inset(kHighlightScreenEdgePaddingDp,
                              kHighlightScreenEdgePaddingDp);

    constexpr gfx::Rect kEmptyRect;
    left_highlight_view_->layer()->SetTransform(gfx::Transform());
    right_highlight_view_->layer()->SetTransform(gfx::Transform());
    if (IsPreviewAreaOnLeftTopOfScreen(indicator_state_)) {
      left_highlight_view_->SetBoundsRect(preview_area_bounds);
      right_highlight_view_->SetBoundsRect(kEmptyRect);
    } else if (IsPreviewAreaState(indicator_state_)) {
      left_highlight_view_->SetBoundsRect(kEmptyRect);
      right_highlight_view_->SetBoundsRect(preview_area_bounds);
    } else {
      left_highlight_view_->SetBoundsRect(left_highlight_bounds_);
      right_highlight_view_->SetBoundsRect(right_highlight_bounds_);
    }
  }

 private:
  std::vector<RoundedRectView*> GetHighlightViews() {
    return {left_highlight_view_, right_highlight_view_};
  }
  std::vector<RotatedImageLabelView*> GetTextViews() {
    return {left_rotated_view_, right_rotated_view_};
  }

  RoundedRectView* left_highlight_view_ = nullptr;
  RoundedRectView* right_highlight_view_ = nullptr;
  RotatedImageLabelView* left_rotated_view_ = nullptr;
  RotatedImageLabelView* right_rotated_view_ = nullptr;

  // Cache the bounds calculated in Layout(), so that they do not have to be
  // recalculated when animation is finished.
  gfx::Rect left_highlight_bounds_;
  gfx::Rect right_highlight_bounds_;

  IndicatorState indicator_state_ = IndicatorState::kNone;
  IndicatorState previous_indicator_state_ = IndicatorState::kNone;

  DISALLOW_COPY_AND_ASSIGN(SplitViewDragIndicatorsView);
};

SplitViewDragIndicators::SplitViewDragIndicators() {
  indicators_view_ = new SplitViewDragIndicatorsView();
  widget_ = CreateWidget();
  widget_->SetContentsView(indicators_view_);
  widget_->Show();
}

SplitViewDragIndicators::~SplitViewDragIndicators() = default;

void SplitViewDragIndicators::SetIndicatorState(
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
    widget_->SetContentsView(indicators_view_);
  }
  gfx::Rect bounds = screen_util::GetDisplayWorkAreaBoundsInParent(
      root_window->GetChildById(kShellWindowId_OverlayContainer));
  ::wm::ConvertRectToScreen(root_window, &bounds);
  widget_->SetBounds(bounds);

  current_indicator_state_ = indicator_state;
  indicators_view_->OnIndicatorTypeChanged(current_indicator_state_);
}

void SplitViewDragIndicators::OnDisplayBoundsChanged() {
  aura::Window* root_window = widget_->GetNativeView()->GetRootWindow();
  gfx::Rect bounds = screen_util::GetDisplayWorkAreaBoundsInParent(
      root_window->GetChildById(kShellWindowId_OverlayContainer));
  ::wm::ConvertRectToScreen(root_window, &bounds);
  widget_->SetBounds(bounds);
}

bool SplitViewDragIndicators::GetIndicatorTypeVisibilityForTesting(
    IndicatorType type) const {
  return indicators_view_->GetViewForIndicatorType(type)->layer()->opacity() >
         0.f;
}

}  // namespace ash
