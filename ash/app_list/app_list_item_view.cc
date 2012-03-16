// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_item_view.h"

#include "ash/app_list/app_list_item_model.h"
#include "ash/app_list/app_list_model_view.h"
#include "ash/app_list/drop_shadow_label.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace ash {

namespace {

const int kPadding = 10;
const int kIconTitleSpacing = 10;
const int kMinLabelWidth = 150;

const SkColor kTitleColor = SK_ColorWHITE;

// 0.2 white
const SkColor kHoverAndPushedColor = SkColorSetARGB(0x33, 0xFF, 0xFF, 0xFF);

// 0.1 white
const SkColor kSelectedColor = SkColorSetARGB(0x20, 0xFF, 0xFF, 0xFF);

const SkColor kHighlightedColor = kHoverAndPushedColor;

gfx::Font GetTitleFont() {
  static gfx::Font* font = NULL;
  if (!font) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    font = new gfx::Font(rb.GetFont(ResourceBundle::BaseFont).DeriveFont(
        1, gfx::Font::BOLD));
  }
  return *font;
}

// An image view that is not interactive.
class StaticImageView : public views::ImageView {
 public:
  StaticImageView() : ImageView() {
  }

 private:
  // views::View overrides:
  virtual bool HitTest(const gfx::Point& l) const OVERRIDE {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(StaticImageView);
};

}  // namespace

// static
const char AppListItemView::kViewClassName[] = "ash/app_list/AppListItemView";

AppListItemView::AppListItemView(AppListModelView* list_model_view,
                                 AppListItemModel* model,
                                 views::ButtonListener* listener)
    : CustomButton(listener),
      model_(model),
      list_model_view_(list_model_view),
      icon_(new StaticImageView),
      title_(new DropShadowLabel),
      selected_(false) {
  title_->SetFont(GetTitleFont());
  title_->SetBackgroundColor(0);
  title_->SetEnabledColor(kTitleColor);

  AddChildView(icon_);
  AddChildView(title_);

  ItemIconChanged();
  ItemTitleChanged();
  model_->AddObserver(this);

  set_context_menu_controller(this);
  set_request_focus_on_press(false);
}

AppListItemView::~AppListItemView() {
  model_->RemoveObserver(this);
}

// static
gfx::Size AppListItemView::GetPreferredSizeForIconSize(
    const gfx::Size& icon_size) {
  gfx::Size size(
      std::max(icon_size.width() + icon_size.width() / 2, kMinLabelWidth),
      icon_size.height() + kIconTitleSpacing + GetTitleFont().GetHeight());
  size.Enlarge(2 * kPadding, 2 * kPadding);
  return size;
}

void AppListItemView::SetSelected(bool selected) {
  if (selected == selected_)
    return;

  selected_ = selected;
  SchedulePaint();
}

void AppListItemView::ItemIconChanged() {
  icon_->SetImage(model_->icon());
}

void AppListItemView::ItemTitleChanged() {
  title_->SetText(UTF8ToUTF16(model_->title()));
}

void AppListItemView::ItemHighlightedChanged() {
  SchedulePaint();
}

std::string AppListItemView::GetClassName() const {
  return kViewClassName;
}

gfx::Size AppListItemView::GetPreferredSize() {
  return GetPreferredSizeForIconSize(icon_size_);
}

void AppListItemView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  rect.Inset(kPadding, kPadding);
  gfx::Size title_size = title_->GetPreferredSize();

  icon_->SetImageSize(icon_size_);
  icon_->SetBounds(rect.x(),
                   rect.y(),
                   rect.width(),
                   icon_size_.height());

  title_->SetBounds(rect.x(),
                    icon_->bounds().bottom() + kIconTitleSpacing,
                    rect.width(),
                    title_size.height());
}

void AppListItemView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());

  if (model_->highlighted()) {
    canvas->FillRect(rect, kHighlightedColor);
  } else if (hover_animation_->is_animating()) {
    int alpha = SkColorGetA(kHoverAndPushedColor) *
        hover_animation_->GetCurrentValue();
    canvas->FillRect(rect, SkColorSetA(kHoverAndPushedColor, alpha));
  } else if (state() == BS_HOT || state() == BS_PUSHED) {
    canvas->FillRect(rect, kHoverAndPushedColor);
  } else if (selected_) {
    canvas->FillRect(rect, kSelectedColor);
  }
}

void AppListItemView::ShowContextMenuForView(views::View* source,
                                             const gfx::Point& point) {
  ui::MenuModel* menu_model = model_->GetContextMenuModel();
  if (!menu_model)
    return;

  views::MenuModelAdapter menu_adapter(menu_model);
  context_menu_runner_.reset(
      new views::MenuRunner(new views::MenuItemView(&menu_adapter)));
  menu_adapter.BuildMenu(context_menu_runner_->GetMenu());
  if (context_menu_runner_->RunMenuAt(
          GetWidget(), NULL, gfx::Rect(point, gfx::Size()),
          views::MenuItemView::TOPLEFT, views::MenuRunner::HAS_MNEMONICS) ==
      views::MenuRunner::MENU_DELETED)
    return;
}

void AppListItemView::StateChanged() {
  if (state() == BS_HOT || state() == BS_PUSHED) {
    list_model_view_->SetSelectedItem(this);
  } else {
    list_model_view_->ClearSelectedItem(this);
    model_->SetHighlighted(false);
  }
}

}  // namespace ash
