// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/special_popup_row.h"

#include "ash/common/ash_constants.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/system_menu_button.h"
#include "ash/common/system/tray/throbber_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_header_button.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"

namespace ash {
namespace {

const int kIconPaddingLeft = 5;
const int kSeparatorInset = 10;
const int kSpecialPopupRowHeight = 55;
const int kSpecialPopupRowHeightMd = 48;
const int kBorderHeight = 1;
const SkColor kBorderColor = SkColorSetRGB(0xaa, 0xaa, 0xaa);

views::View* CreateViewContainer() {
  views::View* view = new views::View;
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    views::BoxLayout* box_layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 4, 0, 0);
    box_layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
    box_layout->set_cross_axis_alignment(
        views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
    view->SetLayoutManager(box_layout);
  } else {
    view->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
    view->SetBorder(views::Border::CreateEmptyBorder(4, 0, 4, 5));
  }

  return view;
}

}  // namespace

SpecialPopupRow::SpecialPopupRow()
    : views_before_content_container_(nullptr),
      content_(nullptr),
      views_after_content_container_(nullptr),
      label_(nullptr) {
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    SetBorder(views::Border::CreateSolidSidedBorder(0, 0, kBorderHeight, 0,
                                                    kBorderColor));
  } else {
    set_background(
        views::Background::CreateSolidBackground(kHeaderBackgroundColor));
    SetBorder(views::Border::CreateSolidSidedBorder(kBorderHeight, 0, 0, 0,
                                                    kBorderColor));
  }
}

SpecialPopupRow::~SpecialPopupRow() {}

void SpecialPopupRow::SetTextLabel(int string_id, ViewClickListener* listener) {
  if (MaterialDesignController::IsSystemTrayMenuMaterial())
    SetTextLabelMd(string_id, listener);
  else
    SetTextLabelNonMd(string_id, listener);
}

void SpecialPopupRow::SetContent(views::View* view) {
  CHECK(!content_);
  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
  SetLayoutManager(box_layout);
  content_ = view;
  // TODO(tdanderson): Consider moving this logic to a BoxLayout subclass.
  AddChildViewAt(content_, views_before_content_container_ ? 1 : 0);
  if (MaterialDesignController::IsSystemTrayMenuMaterial())
    box_layout->SetFlexForView(content_, 1);
}

views::Button* SpecialPopupRow::AddBackButton(views::ButtonListener* listener) {
  SystemMenuButton* button = new SystemMenuButton(
      listener, kSystemMenuArrowBackIcon, IDS_ASH_STATUS_TRAY_PREVIOUS_MENU);
  AddViewBeforeContent(button);
  return button;
}

views::CustomButton* SpecialPopupRow::AddSettingsButton(
    views::ButtonListener* listener,
    LoginStatus status) {
  SystemMenuButton* button = new SystemMenuButton(
      listener, kSystemMenuSettingsIcon, IDS_ASH_STATUS_TRAY_SETTINGS);
  if (!CanOpenWebUISettings(status))
    button->SetState(views::Button::STATE_DISABLED);
  AddViewAfterContent(button);
  return button;
}

views::CustomButton* SpecialPopupRow::AddHelpButton(
    views::ButtonListener* listener,
    LoginStatus status) {
  SystemMenuButton* button = new SystemMenuButton(listener, kSystemMenuHelpIcon,
                                                  IDS_ASH_STATUS_TRAY_HELP);
  if (!CanOpenWebUISettings(status))
    button->SetState(views::Button::STATE_DISABLED);
  AddViewAfterContent(button);
  return button;
}

views::ToggleButton* SpecialPopupRow::AddToggleButton(
    views::ButtonListener* listener) {
  // TODO(tdanderson): Define the focus rect for ToggleButton.
  views::ToggleButton* toggle = new views::ToggleButton(listener);
  toggle->SetFocusForPlatform();

  views::View* container = new views::View;
  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  container->SetLayoutManager(layout);

  container->AddChildView(toggle);
  AddViewAfterContent(container);

  return toggle;
}

void SpecialPopupRow::AddViewToTitleRow(views::View* view) {
  AddViewAfterContent(view);
}

void SpecialPopupRow::AddViewToRowNonMd(views::View* view, bool add_separator) {
  AddViewAfterContent(view, add_separator);
}

gfx::Size SpecialPopupRow::GetPreferredSize() const {
  gfx::Size size = views::View::GetPreferredSize();
  size.set_height(MaterialDesignController::IsSystemTrayMenuMaterial()
                      ? kSpecialPopupRowHeightMd + GetInsets().height()
                      : kSpecialPopupRowHeight);
  return size;
}

int SpecialPopupRow::GetHeightForWidth(int width) const {
  return MaterialDesignController::IsSystemTrayMenuMaterial()
             ? kSpecialPopupRowHeightMd + GetInsets().height()
             : kSpecialPopupRowHeight;
}

void SpecialPopupRow::Layout() {
  views::View::Layout();

  if (MaterialDesignController::IsSystemTrayMenuMaterial())
    return;

  const gfx::Rect content_bounds = GetContentsBounds();
  if (content_bounds.IsEmpty())
    return;

  if (!views_after_content_container_) {
    content_->SetBoundsRect(GetContentsBounds());
    return;
  }

  gfx::Rect bounds(views_after_content_container_->GetPreferredSize());
  bounds.set_height(content_bounds.height());

  gfx::Rect container_bounds = content_bounds;
  container_bounds.ClampToCenteredSize(bounds.size());
  container_bounds.set_x(content_bounds.width() - container_bounds.width());
  views_after_content_container_->SetBoundsRect(container_bounds);

  bounds = content_->bounds();
  bounds.set_width(views_after_content_container_->x());
  content_->SetBoundsRect(bounds);
}

void SpecialPopupRow::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  views::View::OnNativeThemeChanged(theme);
  UpdateStyle();
}

void SpecialPopupRow::UpdateStyle() {
  if (!MaterialDesignController::IsSystemTrayMenuMaterial() || !label_)
    return;

  // TODO(tdanderson|bruthig): Consider changing the SpecialPopupRow
  // constructor to accept information about the row's style (e.g.,
  // FontStyle, variations in padding values, etc).
  TrayPopupItemStyle style(GetNativeTheme(),
                           TrayPopupItemStyle::FontStyle::TITLE);
  style.SetupLabel(label_);
}

void SpecialPopupRow::AddViewBeforeContent(views::View* view) {
  if (!views_before_content_container_) {
    views_before_content_container_ = CreateViewContainer();
    AddChildViewAt(views_before_content_container_, 0);
  }
  views_before_content_container_->AddChildView(view);
}

void SpecialPopupRow::AddViewAfterContent(views::View* view) {
  AddViewAfterContent(view, false);
}

void SpecialPopupRow::AddViewAfterContent(views::View* view,
                                          bool add_separator) {
  if (!views_after_content_container_) {
    views_after_content_container_ = CreateViewContainer();
    AddChildView(views_after_content_container_);
  }

  if (add_separator) {
    views::Separator* separator =
        new views::Separator(views::Separator::VERTICAL);
    separator->SetColor(ash::kBorderDarkColor);
    separator->SetBorder(views::Border::CreateEmptyBorder(kSeparatorInset, 0,
                                                          kSeparatorInset, 0));
    views_after_content_container_->AddChildView(separator);
  }

  views_after_content_container_->AddChildView(view);
}

void SpecialPopupRow::SetTextLabelMd(int string_id,
                                     ViewClickListener* listener) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  views::View* container = new views::View;

  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 4, 4, 0);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  container->SetLayoutManager(box_layout);

  label_ = new views::Label(rb.GetLocalizedString(string_id));
  container->AddChildView(label_);
  UpdateStyle();

  SetContent(container);
}

void SpecialPopupRow::SetTextLabelNonMd(int string_id,
                                        ViewClickListener* listener) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  HoverHighlightView* container = new HoverHighlightView(listener);
  container->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 3, kIconPaddingLeft));

  container->set_highlight_color(SkColorSetARGB(0, 0, 0, 0));
  container->set_default_color(SkColorSetARGB(0, 0, 0, 0));
  container->set_text_highlight_color(kHeaderTextColorHover);
  container->set_text_default_color(kHeaderTextColorNormal);

  container->AddIconAndLabel(
      *rb.GetImageNamed(IDR_AURA_UBER_TRAY_LESS).ToImageSkia(),
      rb.GetLocalizedString(string_id), true /* highlight */);

  container->SetBorder(
      views::Border::CreateEmptyBorder(0, kTrayPopupPaddingHorizontal, 0, 0));

  container->SetAccessibleName(
      rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_PREVIOUS_MENU));
  SetContent(container);
}

}  // namespace ash
