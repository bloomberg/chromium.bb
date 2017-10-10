// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_overview_overlay.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/root_window_finder.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Width in DIP of each highlight view.
constexpr int kHighlightScreenWidth = 200;

// An alpha value for the highlight views.
constexpr SkColor kHighlightBackgroundAlpha = 0x80;

// Color of the labels' background/text.
constexpr SkColor kLabelBackgroundColor = SK_ColorBLACK;
constexpr SkColor kLabelEnabledColor = SK_ColorWHITE;

// Height of the labels.
constexpr int kLabelHeightDp = 40;

// The size of the warning icon in the when a window incompatible with
// splitscreen is dragged.
constexpr int kWarningIconSizeDp = 24;

// The amount of vertical inset to be applied on a rotated image label view.
constexpr int kRotatedViewVerticalInsetDp = 6;

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

// Computes the transform which rotates the labels 90 degrees clockwise or
// anti-clockwise based on |clockwise|. The point of rotation is the relative
// center point of |bounds|.
gfx::Transform ComputeRotateAroundCenterTransform(const gfx::Rect& bounds,
                                                  bool clockwise) {
  gfx::Transform transform;
  const gfx::Vector2dF center_point_vector =
      bounds.CenterPoint() - bounds.origin();
  transform.Translate(center_point_vector);
  transform.Rotate(clockwise ? 90.0 : -90.0);
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

    label_ =
        new views::Label(l10n_util::GetStringUTF16(IDS_ASH_SPLIT_VIEW_GUIDANCE),
                         views::style::CONTEXT_LABEL);
    label_->SetPaintToLayer();
    label_->layer()->SetFillsBoundsOpaquely(false);
    label_->SetEnabledColor(kLabelEnabledColor);
    label_->SetBackgroundColor(kLabelBackgroundColor);

    auto* layout = new views::BoxLayout(views::BoxLayout::kVertical);
    SetLayoutManager(layout);
    SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    layer()->SetColor(SK_ColorBLACK);
    AddChildView(icon_);
    AddChildView(label_);
    layout->SetFlexForView(label_, 1);
    SetBorder(
        views::CreateEmptyBorder(gfx::Insets(kRotatedViewVerticalInsetDp, 0)));
  }

  ~RotatedImageLabelView() override = default;

  void SetLabelText(const base::string16& text) { label_->SetText(text); }

  bool icon_visible() const { return icon_->visible(); }

  void SetIconVisible(bool visible) { icon_->SetVisible(visible); }

 private:
  views::ImageView* icon_ = nullptr;
  views::Label* label_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RotatedImageLabelView);
};

// View which contains two highlights on each side indicator where a user should
// drag a selected window in order to initiate splitview. Each highlight has a
// label with instructions to further guide users.
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

  void Layout() override {
    left_hightlight_view_->SetBounds(0, 0, kHighlightScreenWidth, height());
    right_hightlight_view_->SetBounds(width() - kHighlightScreenWidth, 0,
                                      kHighlightScreenWidth, height());

    const int rotated_bounds_height =
        kLabelHeightDp +
        (left_rotated_view_->icon_visible() ? kWarningIconSizeDp : 0);
    const gfx::Rect rotated_bounds(0, height() / 2 - rotated_bounds_height / 2,
                                   kHighlightScreenWidth,
                                   rotated_bounds_height);
    left_rotated_view_->SetBoundsRect(rotated_bounds);
    right_rotated_view_->SetBoundsRect(rotated_bounds);

    left_rotated_view_->SetTransform(ComputeRotateAroundCenterTransform(
        rotated_bounds, false /* clockwise */));
    right_rotated_view_->SetTransform(ComputeRotateAroundCenterTransform(
        rotated_bounds, true /* clockwise */));
  }

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

 private:
  void SetHighlightsVisible(bool visible) {
    left_hightlight_view_->SetVisible(visible);
    right_hightlight_view_->SetVisible(visible);
  }

  void SetLabelsText(const base::string16& text) {
    left_rotated_view_->SetLabelText(text);
    right_rotated_view_->SetLabelText(text);
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

  IndicatorType indicator_type_;

  DISALLOW_COPY_AND_ASSIGN(SplitViewOverviewOverlayView);
};

SplitViewOverviewOverlay::SplitViewOverviewOverlay() {
  overlay_view_ = new SplitViewOverviewOverlayView();
  widget_ = CreateWidget();
  widget_->SetContentsView(overlay_view_);
}

SplitViewOverviewOverlay::~SplitViewOverviewOverlay() {}

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
  widget_->SetBounds(root_window->GetBoundsInScreen());
  widget_->Show();
  overlay_view_->OnIndicatorTypeChanged(current_indicator_type_);
}

}  // namespace ash
