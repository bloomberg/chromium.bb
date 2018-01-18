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

// An alpha value for the highlight views.
constexpr SkColor kHighlightBackgroundAlpha = 0x4D;

// Creates the widget responsible for displaying the indicators.
std::unique_ptr<views::Widget> CreateWidget() {
  std::unique_ptr<views::Widget> widget(new views::Widget);
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
// the display in landscape mode.
class SplitViewOverviewOverlay::SplitViewOverviewOverlayView
    : public views::View {
 public:
  SplitViewOverviewOverlayView() {
    left_hightlight_view_ =
        new RoundedRectView(kHighlightScreenRoundRectRadiusDp, SK_ColorWHITE);
    right_hightlight_view_ =
        new RoundedRectView(kHighlightScreenRoundRectRadiusDp, SK_ColorWHITE);

    AddChildView(left_hightlight_view_);
    AddChildView(right_hightlight_view_);

    left_rotated_view_ = new RotatedImageLabelView();
    right_rotated_view_ = new RotatedImageLabelView();

    left_hightlight_view_->AddChildView(left_rotated_view_);
    right_hightlight_view_->AddChildView(right_rotated_view_);
  }

  ~SplitViewOverviewOverlayView() override = default;

  void OnIndicatorTypeChanged(IndicatorType indicator_type) {
    if (indicator_type_ == indicator_type)
      return;

    indicator_type_ = indicator_type;
    switch (indicator_type) {
      case IndicatorType::NONE:
        return;
      case IndicatorType::DRAG_AREA:
      case IndicatorType::CANNOT_SNAP:
        SetLabelsText(l10n_util::GetStringUTF16(
            indicator_type == IndicatorType::CANNOT_SNAP
                ? IDS_ASH_SPLIT_VIEW_CANNOT_SNAP
                : IDS_ASH_SPLIT_VIEW_GUIDANCE));
        const SkColor color = indicator_type == IndicatorType::CANNOT_SNAP
                                  ? SK_ColorBLACK
                                  : SK_ColorWHITE;
        left_hightlight_view_->SetBackgroundColor(
            SkColorSetA(color, kHighlightBackgroundAlpha));
        right_hightlight_view_->SetBackgroundColor(
            SkColorSetA(color, kHighlightBackgroundAlpha));
        return;
    }

    NOTREACHED();
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
    left_hightlight_view_->SetBounds(kHighlightScreenEdgePaddingDp,
                                     kHighlightScreenEdgePaddingDp,
                                     highlight_width, highlight_height);
    right_hightlight_view_->SetBounds(right_bottom_origin.x(),
                                      right_bottom_origin.y(), highlight_width,
                                      highlight_height);

    // Calculate the bounds of the views which contain the guidance text and
    // icon. Rotate the two views in landscape mode.
    const gfx::Size size(
        left_rotated_view_->GetPreferredSize().width(),
        std::max(kSplitviewLabelPreferredHeightDp,
                 left_rotated_view_->GetPreferredSize().height()));
    const gfx::Rect rotated_bounds(highlight_width / 2 - size.width() / 2,
                                   highlight_height / 2 - size.height() / 2,
                                   size.width(), size.height());

    // In portrait mode, there is no need to rotate the text and warning icon.
    // In landscape mode, rotate the left text 90 degrees clockwise in rtl and
    // 90 degress anti clockwise in ltr. The right text is rotated 90 degrees in
    // the opposite direction of the left text.
    double left_rotation_angle = 0.0;
    if (landscape)
      left_rotation_angle = 90.0 * (base::i18n::IsRTL() ? 1 : -1);
    left_rotated_view_->OnBoundsUpdated(rotated_bounds,
                                        left_rotation_angle /* angle */);
    right_rotated_view_->OnBoundsUpdated(rotated_bounds,
                                         -left_rotation_angle /* angle */);
  }

 private:
  void SetLabelsText(const base::string16& text) {
    left_rotated_view_->SetLabelText(text);
    right_rotated_view_->SetLabelText(text);
    Layout();
  }

  RoundedRectView* left_hightlight_view_ = nullptr;
  RoundedRectView* right_hightlight_view_ = nullptr;
  RotatedImageLabelView* left_rotated_view_ = nullptr;
  RotatedImageLabelView* right_rotated_view_ = nullptr;

  IndicatorType indicator_type_ = IndicatorType::NONE;

  DISALLOW_COPY_AND_ASSIGN(SplitViewOverviewOverlayView);
};

SplitViewOverviewOverlay::SplitViewOverviewOverlay() {
  overlay_view_ = new SplitViewOverviewOverlayView();
  widget_ = CreateWidget();
  widget_->SetContentsView(overlay_view_);
  widget_->Show();
  widget_->GetLayer()->SetOpacity(0.f);
}

SplitViewOverviewOverlay::~SplitViewOverviewOverlay() = default;

void SplitViewOverviewOverlay::SetIndicatorType(
    IndicatorType indicator_type,
    const gfx::Point& event_location) {
  if (indicator_type == current_indicator_type_)
    return;

  current_indicator_type_ = indicator_type;
  // Only show the overlay if nothing is snapped.
  if (Shell::Get()->split_view_controller()->state() !=
      SplitViewController::NO_SNAP) {
    current_indicator_type_ = IndicatorType::NONE;
  }
  const bool visible = current_indicator_type_ != IndicatorType::NONE;
  if (!visible) {
    AnimateSplitviewLabelOpacity(widget_->GetLayer(), visible);
    return;
  }

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
  AnimateSplitviewLabelOpacity(widget_->GetLayer(), visible);
  overlay_view_->OnIndicatorTypeChanged(current_indicator_type_);
}

void SplitViewOverviewOverlay::OnDisplayBoundsChanged() {
  overlay_view_->Layout();
}

}  // namespace ash
