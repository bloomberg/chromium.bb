// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_overflow_indicator_view.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/ui/panels/panel_browser_frame_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/path.h"
#include "ui/views/background.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace {

enum OverflowPaintState {
  PAINT_AS_NORMAL,
  PAINT_FOR_ATTENTION
};

// The height in pixels of the widget to show the plus count.
const int kWidgetHeight = 15;

// The radius of the corner of the border.
const int kCornerRadius = 4;

// Metrics used to draw the text.
const int kTextFontSize = 10;
const SkColor kTextColor = SK_ColorWHITE;

// Gradient colors used to draw the background in normal mode.
const SkColor kNormalBackgroundColorStart = 0xff777777;
const SkColor kNormalBackgroundColorEnd = 0xff555555;

// Gradient colors used to draw the background in attention mode.
const SkColor kAttentionBackgroundColorStart = 0xffffab57;
const SkColor kAttentionBackgroundColorEnd = 0xfff59338;

views::Painter* GetBackgroundPainter(OverflowPaintState paint_state) {
  static views::Painter* normal_painter = NULL;
  static views::Painter* attention_painter = NULL;

  if (paint_state == PAINT_AS_NORMAL) {
    if (!normal_painter) {
      normal_painter = views::Painter::CreateVerticalGradient(
          kNormalBackgroundColorStart, kNormalBackgroundColorEnd);
    }
    return normal_painter;
  } else {
    if (!attention_painter) {
      attention_painter = views::Painter::CreateVerticalGradient(
          kAttentionBackgroundColorStart, kAttentionBackgroundColorEnd);
    }
    return attention_painter;
  }
}

}  // namespace

PanelOverflowIndicator* PanelOverflowIndicator::Create() {
  return new PanelOverflowIndicatorView();
}

PanelOverflowIndicatorView::PanelOverflowIndicatorView()
    : count_(0),
      is_drawing_attention_(false) {
  widget_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.keep_on_top = true;
  params.can_activate = false;
  params.transparent = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget_->Init(params);
  widget_->SetAlwaysOnTop(true);

  const gfx::Font& base_font =
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);
  title_.SetFont(base_font.DeriveFont(
      kTextFontSize - base_font.GetFontSize(), gfx::Font::BOLD));
  title_.SetHorizontalAlignment(views::Label::ALIGN_CENTER);
  title_.SetAutoColorReadabilityEnabled(false);

  widget_->SetContentsView(&title_);
  widget_->Show();

  PaintBackground();
}

PanelOverflowIndicatorView::~PanelOverflowIndicatorView() {
}

int PanelOverflowIndicatorView::GetHeight() const {
  return kWidgetHeight;
}

gfx::Rect PanelOverflowIndicatorView::GetBounds() const {
  return widget_->GetWindowScreenBounds();
}

void PanelOverflowIndicatorView::SetBounds(const gfx::Rect& bounds) {
  widget_->SetBounds(bounds);

  gfx::Path path;
  GetFrameMask(bounds, &path);
  widget_->SetShape(path.CreateNativeRegion());
}

int PanelOverflowIndicatorView::GetCount() const {
  return count_;
}

void PanelOverflowIndicatorView::SetCount(int count) {
  if (count_ == count)
    return;
  count_ = count;

  if (count_ > 0)
    widget_->Show();
  else
    widget_->Hide();

  UpdateTitle();
}

void PanelOverflowIndicatorView::DrawAttention() {
  if (is_drawing_attention_)
    return;
  is_drawing_attention_ = true;
  PaintBackground();
}

void PanelOverflowIndicatorView::StopDrawingAttention() {
  if (!is_drawing_attention_)
    return;
  is_drawing_attention_ = false;
  PaintBackground();
}

bool PanelOverflowIndicatorView::IsDrawingAttention() const {
  return is_drawing_attention_;
}

void PanelOverflowIndicatorView::UpdateTitle() {
  string16 text(L"+");
  text += base::IntToString16(count_);
  title_.SetText(text);
}

void PanelOverflowIndicatorView::PaintBackground() {
  OverflowPaintState paint_state = is_drawing_attention_ ? PAINT_FOR_ATTENTION
                                                         : PAINT_AS_NORMAL;

  title_.set_background(views::Background::CreateBackgroundPainter(
      false, GetBackgroundPainter(paint_state)));
  title_.SetEnabledColor(kTextColor);
  title_.SchedulePaint();  // SetEnabledColor does not call SchedulePaint.
}

void PanelOverflowIndicatorView::GetFrameMask(const gfx::Rect& rect,
                                         gfx::Path* path) const {
  SkScalar radius = SkIntToScalar(kCornerRadius);
  SkScalar spline_radius = radius -
      SkScalarMul(radius, (SK_ScalarSqrt2 - SK_Scalar1) * 4 / 3);
  SkScalar left = SkIntToScalar(0);
  SkScalar top = SkIntToScalar(0);
  SkScalar right = SkIntToScalar(rect.width());
  SkScalar bottom = SkIntToScalar(rect.height());

  path->moveTo(left + radius, top);
  path->lineTo(right - radius, top);
  path->cubicTo(right - spline_radius, top,
                right, top + spline_radius,
                right, top + radius);
  path->lineTo(right, bottom);
  path->lineTo(left, bottom);
  path->lineTo(left, top + radius);
  path->cubicTo(left, top + spline_radius,
                left + spline_radius, top,
                left + radius, top);
  path->close();
}
