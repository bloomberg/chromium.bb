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

const int kTopBottomPadding = 10;
const int kLeftRightPadding = 20;
const int kIconTitleSpacing = 10;

const SkColor kTitleColor = SK_ColorWHITE;

// 0.33 black
const SkColor kHoverAndPushedColor = SkColorSetARGB(0x55, 0x00, 0x00, 0x00);

// 0.16 black
const SkColor kSelectedColor = SkColorSetARGB(0x2A, 0x00, 0x00, 0x00);

const SkColor kHighlightedColor = kHoverAndPushedColor;

// FontSize/IconSize ratio = 24 / 128, which means we should get 24 font size
// when icon size is 128.
const float kFontSizeToIconSizeRatio = 0.1875f;

// Font smaller than kBoldFontSize needs to be bold.
const int kBoldFontSize = 14;

const int kMinFontSize = 12;

const int kMinTitleChars = 15;

const gfx::Font& GetTitleFontForIconSize(const gfx::Size& size) {
  static int icon_height;
  static gfx::Font* font = NULL;

  if (font && icon_height == size.height())
    return *font;

  delete font;

  icon_height = size.height();
  int font_size = std::max(
      static_cast<int>(icon_height * kFontSizeToIconSizeRatio),
      kMinFontSize);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::Font title_font(rb.GetFont(ui::ResourceBundle::BaseFont).GetFontName(),
                       font_size);
  if (font_size <= kBoldFontSize)
    title_font = title_font.DeriveFont(0, gfx::Font::BOLD);
  font = new gfx::Font(title_font);
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

// A minimum title width set by test to override the default logic that derives
// the min width from font.
int g_min_title_width = 0;

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
  title_->SetBackgroundColor(0);
  title_->SetEnabledColor(kTitleColor);
  title_->SetDropShadowSize(3);

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
  int min_title_width = g_min_title_width;
  if (min_title_width == 0) {
    const gfx::Font& title_font = GetTitleFontForIconSize(icon_size);
    min_title_width = kMinTitleChars * title_font.GetAverageCharacterWidth();
  }

  int dimension = std::max(icon_size.width() * 2, min_title_width);
  gfx::Size size(dimension, dimension);
  size.Enlarge(kLeftRightPadding, kTopBottomPadding);
  return size;
}

// static
void AppListItemView::SetMinTitleWidth(int width) {
  g_min_title_width = width;
}

void AppListItemView::SetIconSize(const gfx::Size& size) {
  icon_size_ = size;
  title_->SetFont(GetTitleFontForIconSize(size));
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
  rect.Inset(kLeftRightPadding, kTopBottomPadding);
  gfx::Size title_size = title_->GetPreferredSize();
  int height = icon_size_.height() + kIconTitleSpacing +
      title_size.height();
  int y = rect.y() + (rect.height() - height) / 2;

  icon_->SetImageSize(icon_size_);
  icon_->SetBounds(rect.x(), y, rect.width(), icon_size_.height());

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
