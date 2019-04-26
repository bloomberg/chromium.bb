// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/rounded_label_widget.h"

#include "ash/public/cpp/ash_features.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// The contents of RoundedLabelWidget. It is a rounded background with a label
// containing text we want to display.
class RoundedLabelView : public views::View {
 public:
  RoundedLabelView(int horizontal_padding,
                   int vertical_padding,
                   SkColor background_color,
                   SkColor foreground_color,
                   int rounding_dp,
                   int preferred_height,
                   int message_id)
      : horizontal_padding_(horizontal_padding),
        preferred_height_(preferred_height) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical,
        gfx::Insets(vertical_padding, horizontal_padding)));
    SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    layer()->SetColor(background_color);
    layer()->SetFillsBoundsOpaquely(false);

    if (ash::features::ShouldUseShaderRoundedCorner()) {
      const std::array<uint32_t, 4> kRadii = {rounding_dp, rounding_dp,
                                              rounding_dp, rounding_dp};
      layer()->SetRoundedCornerRadius(kRadii);
      layer()->SetIsFastRoundedCorner(true);
    }

    label_ = new views::Label(l10n_util::GetStringUTF16(message_id),
                              views::style::CONTEXT_LABEL);
    label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    label_->SetEnabledColor(foreground_color);
    label_->SetBackgroundColor(background_color);
    label_->SetPaintToLayer();
    label_->layer()->SetFillsBoundsOpaquely(false);
    AddChildView(label_);
  }
  ~RoundedLabelView() override = default;

  // views::View:
  gfx::Size GetPreferredSize() {
    int width = label_->GetPreferredSize().width() + 2 * horizontal_padding_;
    return gfx::Size(width, preferred_height_);
  }

 private:
  views::Label* label_ = nullptr;  // Owned by views hierarchy.

  int horizontal_padding_;
  int preferred_height_;

  DISALLOW_COPY_AND_ASSIGN(RoundedLabelView);
};

}  // namespace

RoundedLabelWidget::RoundedLabelWidget() = default;

RoundedLabelWidget::~RoundedLabelWidget() = default;

void RoundedLabelWidget::Init(const InitParams& params) {
  views::Widget::InitParams widget_params;
  widget_params.type = views::Widget::InitParams::TYPE_POPUP;
  widget_params.keep_on_top = false;
  widget_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget_params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  widget_params.layer_type = ui::LAYER_NOT_DRAWN;
  widget_params.accept_events = false;
  widget_params.parent = params.parent;
  set_focus_on_creation(false);
  views::Widget::Init(widget_params);

  SetContentsView(new RoundedLabelView(
      params.horizontal_padding, params.vertical_padding,
      params.background_color, params.foreground_color, params.rounding_dp,
      params.preferred_height, params.message_id));
  Show();
}

void RoundedLabelWidget::SetBoundsCenteredIn(const gfx::Rect& bounds) {
  DCHECK(GetContentsView());
  RoundedLabelView* contents_view =
      static_cast<RoundedLabelView*>(GetContentsView());
  gfx::Rect widget_bounds = bounds;
  widget_bounds.ClampToCenteredSize(contents_view->GetPreferredSize());
  GetNativeWindow()->SetBounds(widget_bounds);
}

}  // namespace ash
