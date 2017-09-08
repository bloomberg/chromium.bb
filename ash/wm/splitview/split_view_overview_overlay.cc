// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_overview_overlay.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/root_window_finder.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Width in DIP of each highlight view.
constexpr int kHighlightScreenWidth = 200;

// An alpha value for the highlight views.
constexpr SkColor kHighlightBackgroundAlpha = 0x80;

// Color of the labels' background/text.
constexpr SkColor kLabelBackgroundColor = SK_ColorWHITE;
constexpr SkColor kLabelEnabledColor = SK_ColorBLACK;

// Height of the label.
constexpr int kLabelHeightDp = 40;

// Creates the widget responsible for displaying the indicators.
std::unique_ptr<views::Widget> CreateWidget() {
  std::unique_ptr<views::Widget> widget(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.keep_on_top = false;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = false;
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

    // Function for creating the two labels.
    auto label_creator = []() {
      auto* label = new views::Label(
          l10n_util::GetStringUTF16(IDS_ASH_SPLIT_VIEW_GUIDANCE),
          views::style::CONTEXT_LABEL);
      label->SetEnabledColor(kLabelEnabledColor);
      label->SetBackgroundColor(kLabelBackgroundColor);
      label->SetPaintToLayer();
      label->layer()->SetFillsBoundsOpaquely(false);
      return label;
    };
    left_label_ = label_creator();
    right_label_ = label_creator();
    left_hightlight_view_->AddChildView(left_label_);
    right_hightlight_view_->AddChildView(right_label_);
  }

  ~SplitViewOverviewOverlayView() override = default;

  void Layout() override {
    left_hightlight_view_->SetBounds(0, 0, kHighlightScreenWidth, height());
    right_hightlight_view_->SetBounds(width() - kHighlightScreenWidth, 0,
                                      kHighlightScreenWidth, height());

    const gfx::Rect label_bounds(0, height() / 2 - kLabelHeightDp / 2,
                                 kHighlightScreenWidth, kLabelHeightDp);
    left_label_->SetBoundsRect(label_bounds);
    right_label_->SetBoundsRect(label_bounds);

    left_label_->SetTransform(
        ComputeRotateAroundCenterTransform(label_bounds, false));
    right_label_->SetTransform(
        ComputeRotateAroundCenterTransform(label_bounds, true));
  }

 private:
  views::View* left_hightlight_view_ = nullptr;
  views::View* right_hightlight_view_ = nullptr;
  views::Label* left_label_ = nullptr;
  views::Label* right_label_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SplitViewOverviewOverlayView);
};

SplitViewOverviewOverlay::SplitViewOverviewOverlay() {
  overlay_view_ = new SplitViewOverviewOverlayView();
  widget_ = CreateWidget();
  widget_->SetContentsView(overlay_view_);
}

SplitViewOverviewOverlay::~SplitViewOverviewOverlay() {}

void SplitViewOverviewOverlay::SetVisible(bool visible,
                                          const gfx::Point& event_location) {
  // Only show the overlay if nothing is snapped.
  if (Shell::Get()->split_view_controller()->state() !=
          SplitViewController::NO_SNAP ||
      !visible) {
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
}

bool SplitViewOverviewOverlay::visible() const {
  return widget_->IsVisible();
}

}  // namespace ash
