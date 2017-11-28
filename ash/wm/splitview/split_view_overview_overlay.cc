// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_overview_overlay.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/root_window_finder.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// Width in DIP of each highlight view.
constexpr int kHighlightScreenWidth = 200;

// An alpha value for the highlight views.
constexpr SkColor kHighlightBackgroundAlpha = 0x80;

// Color of the labels' background/text.
constexpr SkColor kLabelBackgroundColor = SK_ColorBLACK;
constexpr SkColor kLabelEnabledColor = SK_ColorWHITE;

// The size of the warning icon in the when a window incompatible with
// splitscreen is dragged.
constexpr int kWarningIconSizeDp = 24;

// The amount of inset to be applied on a rotated image label view.
constexpr int kRotatedViewInsetDp = 8;

// The amount of round applied to the corners of a rotated image label view.
constexpr int kRotatedViewVerticalRoundRectRadius = 20;

// The color for the rotated image label views.
constexpr SkColor kRotatedViewBackgroundColor =
    SkColorSetA(SK_ColorBLACK, 0xB0);

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

// View which contains a label and an optional icon. Used by and rotated by
// SplitViewOverviewOverlayView.
class SplitViewOverviewOverlay::RotatedImageLabelView : public views::View {
 public:
  RotatedImageLabelView() {
    icon_ = new views::ImageView();
    icon_->SetPaintToLayer();
    icon_->layer()->SetFillsBoundsOpaquely(false);
    icon_->SetPreferredSize(gfx::Size(kWarningIconSizeDp, kWarningIconSizeDp));
    icon_->SetImage(
        gfx::CreateVectorIcon(kSplitviewNosnapWarningIcon, SK_ColorWHITE));
    icon_->SetVisible(false);

    label_ = new views::Label(base::string16(), views::style::CONTEXT_LABEL);
    label_->SetPaintToLayer();
    label_->layer()->SetFillsBoundsOpaquely(false);
    label_->SetEnabledColor(kLabelEnabledColor);
    label_->SetBackgroundColor(kLabelBackgroundColor);

    auto* layout = new views::BoxLayout(views::BoxLayout::kVertical);
    SetLayoutManager(layout);
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    AddChildView(icon_);
    AddChildView(label_);
    layout->SetFlexForView(label_, 1);
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets(kRotatedViewInsetDp, kRotatedViewInsetDp)));
  }

  ~RotatedImageLabelView() override = default;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    // Clip into a rounded rectangle.
    constexpr SkScalar radius =
        SkIntToScalar(kRotatedViewVerticalRoundRectRadius);
    constexpr SkScalar kRadius[8] = {radius, radius, radius, radius,
                                     radius, radius, radius, radius};
    SkPath path;
    path.addRoundRect(gfx::RectToSkRect(gfx::Rect(size())), kRadius);
    canvas->ClipPath(path, true);

    canvas->DrawColor(kRotatedViewBackgroundColor);
  }

  void SetLabelText(const base::string16& text) { label_->SetText(text); }

  bool icon_visible() const { return icon_->visible(); }

  void SetIconVisible(bool visible) { icon_->SetVisible(visible); }

  // Called when the view's bounds are altered. Rotates the view by |angle|
  // degrees.
  void OnBoundsUpdated(const gfx::Rect& bounds, double angle) {
    SetBoundsRect(bounds);
    SetTransform(ComputeRotateAroundCenterTransform(bounds, angle));
  }

 private:
  views::ImageView* icon_ = nullptr;
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
    left_hightlight_view_ = new views::View();
    left_hightlight_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    left_hightlight_view_->layer()->SetColor(
        SkColorSetA(SK_ColorWHITE, kHighlightBackgroundAlpha));

    right_hightlight_view_ = new views::View();
    right_hightlight_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    right_hightlight_view_->layer()->SetColor(
        SkColorSetA(SK_ColorWHITE, kHighlightBackgroundAlpha));
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
        SetHighlightsVisible(false);
        return;
      case IndicatorType::DRAG_AREA:
      case IndicatorType::CANNOT_SNAP:
        SetHighlightsVisible(true);
        SetIconsVisible(indicator_type == IndicatorType::CANNOT_SNAP);
        SetLabelsText(l10n_util::GetStringUTF16(
            indicator_type == IndicatorType::CANNOT_SNAP
                ? IDS_ASH_SPLIT_VIEW_CANNOT_SNAP
                : IDS_ASH_SPLIT_VIEW_GUIDANCE));
        const SkColor color = indicator_type == IndicatorType::CANNOT_SNAP
                                  ? SK_ColorBLACK
                                  : SK_ColorWHITE;
        left_hightlight_view_->layer()->SetColor(
            SkColorSetA(color, kHighlightBackgroundAlpha));
        right_hightlight_view_->layer()->SetColor(
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
    const int highlight_width = landscape ? kHighlightScreenWidth : width();
    const int highlight_height = landscape ? height() : kHighlightScreenWidth;
    const gfx::Point right_bottom_origin(
        landscape ? width() - kHighlightScreenWidth : 0,
        landscape ? 0 : height() - kHighlightScreenWidth);
    left_hightlight_view_->SetBounds(0, 0, highlight_width, highlight_height);
    right_hightlight_view_->SetBounds(right_bottom_origin.x(),
                                      right_bottom_origin.y(), highlight_width,
                                      highlight_height);

    // Calculate the bounds of the views which contain the guidance text and
    // icon. Rotate the two views in landscape mode.
    const gfx::Size size = left_rotated_view_->GetPreferredSize();
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
  void SetHighlightsVisible(bool visible) {
    left_hightlight_view_->SetVisible(visible);
    right_hightlight_view_->SetVisible(visible);
  }

  void SetLabelsText(const base::string16& text) {
    left_rotated_view_->SetLabelText(text);
    right_rotated_view_->SetLabelText(text);
    Layout();
  }

  void SetIconsVisible(bool visible) {
    left_rotated_view_->SetIconVisible(visible);
    right_rotated_view_->SetIconVisible(visible);
    Layout();
  }

  views::View* left_hightlight_view_ = nullptr;
  views::View* right_hightlight_view_ = nullptr;
  RotatedImageLabelView* left_rotated_view_ = nullptr;
  RotatedImageLabelView* right_rotated_view_ = nullptr;

  IndicatorType indicator_type_ = IndicatorType::NONE;

  DISALLOW_COPY_AND_ASSIGN(SplitViewOverviewOverlayView);
};

SplitViewOverviewOverlay::SplitViewOverviewOverlay() {
  overlay_view_ = new SplitViewOverviewOverlayView();
  widget_ = CreateWidget();
  widget_->SetContentsView(overlay_view_);
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
    widget_->Hide();
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
  gfx::Rect bounds = ScreenUtil::GetDisplayWorkAreaBoundsInParent(
      root_window->GetChildById(kShellWindowId_OverlayContainer));
  ::wm::ConvertRectToScreen(root_window, &bounds);
  widget_->SetBounds(bounds);
  widget_->Show();
  overlay_view_->OnIndicatorTypeChanged(current_indicator_type_);
}

void SplitViewOverviewOverlay::OnDisplayBoundsChanged() {
  overlay_view_->Layout();
}

}  // namespace ash
