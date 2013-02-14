// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/special_popup_row.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"

namespace ash {
namespace internal {

namespace {

const int kIconPaddingLeft = 5;
const int kSpecialPopupRowHeight = 55;
const int kBorderHeight = 3;
const SkColor kBorderGradientDark = SkColorSetRGB(0xae, 0xae, 0xae);
const SkColor kBorderGradientLight = SkColorSetRGB(0xe8, 0xe8, 0xe8);

views::View* CreatePopupHeaderButtonsContainer() {
  views::View* view = new views::View;
  view->SetLayoutManager(new
      views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, -1));
  view->set_border(views::Border::CreateEmptyBorder(0, 0, 0, 5));
  return view;
}

class SpecialPopupRowBorder : public views::Border {
 public:
  SpecialPopupRowBorder()
      : painter_(views::Painter::CreateVerticalGradient(kBorderGradientDark,
                                                        kBorderGradientLight)) {
  }

  virtual ~SpecialPopupRowBorder() {}

 private:
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) OVERRIDE {
    views::Painter::PaintPainterAt(canvas, painter_.get(),
        gfx::Rect(gfx::Size(view.width(), kBorderHeight)));
  }

  virtual gfx::Insets GetInsets() const OVERRIDE {
    return gfx::Insets(kBorderHeight, 0, 0, 0);
  }

  scoped_ptr<views::Painter> painter_;

  DISALLOW_COPY_AND_ASSIGN(SpecialPopupRowBorder);
};

}  // namespace

SpecialPopupRow::SpecialPopupRow()
    : content_(NULL),
      button_container_(NULL) {
  views::Background* background = views::Background::CreateBackgroundPainter(
      true, views::Painter::CreateVerticalGradient(kHeaderBackgroundColorLight,
                                                   kHeaderBackgroundColorDark));
  background->SetNativeControlColor(kHeaderBackgroundColorDark);
  set_background(background);
  set_border(new SpecialPopupRowBorder);
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
}

SpecialPopupRow::~SpecialPopupRow() {
}

void SpecialPopupRow::SetTextLabel(int string_id, ViewClickListener* listener) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  HoverHighlightView* container = new HoverHighlightView(listener);
  container->SetLayoutManager(new
      views::BoxLayout(views::BoxLayout::kHorizontal, 0, 3, kIconPaddingLeft));

  container->set_highlight_color(SkColorSetARGB(0, 0, 0, 0));
  container->set_default_color(SkColorSetARGB(0, 0, 0, 0));
  container->set_text_highlight_color(kHeaderTextColorHover);
  container->set_text_default_color(kHeaderTextColorNormal);

  container->AddIconAndLabel(
      *rb.GetImageNamed(IDR_AURA_UBER_TRAY_LESS).ToImageSkia(),
      rb.GetLocalizedString(string_id),
      gfx::Font::BOLD);

  container->set_border(views::Border::CreateEmptyBorder(0,
      kTrayPopupPaddingHorizontal, 0, 0));

  container->SetAccessibleName(
      rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_PREVIOUS_MENU));
  SetContent(container);
}

void SpecialPopupRow::SetContent(views::View* view) {
  CHECK(!content_);
  content_ = view;
  AddChildViewAt(content_, 0);
}

void SpecialPopupRow::AddButton(TrayPopupHeaderButton* button) {
  if (!button_container_) {
    button_container_ = CreatePopupHeaderButtonsContainer();
    AddChildView(button_container_);
  }
  button_container_->AddChildView(button);
}

void SpecialPopupRow::AddThrobber(ThrobberView* throbber) {
  if (!button_container_) {
    button_container_ = CreatePopupHeaderButtonsContainer();
    AddChildView(button_container_);
  }
  button_container_->AddChildView(throbber);
}

gfx::Size SpecialPopupRow::GetPreferredSize() {
  gfx::Size size = views::View::GetPreferredSize();
  size.set_height(kSpecialPopupRowHeight);
  return size;
}

void SpecialPopupRow::Layout() {
  views::View::Layout();
  gfx::Rect content_bounds = GetContentsBounds();
  if (content_bounds.IsEmpty())
    return;
  if (!button_container_) {
    content_->SetBoundsRect(GetContentsBounds());
    return;
  }

  gfx::Rect bounds(button_container_->GetPreferredSize());
  bounds.set_height(content_bounds.height());
  gfx::Rect container_bounds = content_bounds;
  container_bounds.ClampToCenteredSize(bounds.size());
  container_bounds.set_x(content_bounds.width() - container_bounds.width());
  button_container_->SetBoundsRect(container_bounds);

  bounds = content_->bounds();
  bounds.set_width(button_container_->x());
  content_->SetBoundsRect(bounds);
}

}  // namespace internal
}  // namespace ash
