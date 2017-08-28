// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/chosen_object_row.h"

#include "chrome/browser/ui/views/page_info/chosen_object_row_observer.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"

ChosenObjectRow::ChosenObjectRow(
    std::unique_ptr<PageInfoUI::ChosenObjectInfo> info)
    : info_(std::move(info)) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  const int column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::FIXED, kPermissionIconColumnWidth,
                        0);
  column_set->AddPaddingColumn(0, kPermissionIconMarginLeft);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(1, column_set_id);
  // Create the permission icon.
  icon_ = new views::ImageView();
  const gfx::Image& image = PageInfoUI::GetChosenObjectIcon(*info_, false);
  icon_->SetImage(image.ToImageSkia());
  layout->AddView(icon_, 1, 1, views::GridLayout::CENTER,
                  views::GridLayout::CENTER);
  // Create the label that displays the permission type.
  views::Label* label = new views::Label(
      l10n_util::GetStringFUTF16(info_->ui_info.label_string_id,
                                 PageInfoUI::ChosenObjectToUIString(*info_)));
  layout->AddView(label, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::CENTER);
  // Create the delete button.
  delete_button_ = new views::ImageButton(this);
  delete_button_->SetFocusForPlatform();
  delete_button_->set_request_focus_on_press(true);
  delete_button_->SetTooltipText(
      l10n_util::GetStringUTF16(info_->ui_info.delete_tooltip_string_id));
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  delete_button_->SetImage(views::ImageButton::STATE_NORMAL,
                           rb.GetImageSkiaNamed(IDR_CLOSE_2));
  delete_button_->SetImage(views::ImageButton::STATE_HOVERED,
                           rb.GetImageSkiaNamed(IDR_CLOSE_2_H));
  delete_button_->SetImage(views::ImageButton::STATE_PRESSED,
                           rb.GetImageSkiaNamed(IDR_CLOSE_2_P));
  layout->AddView(delete_button_, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::CENTER);
}

void ChosenObjectRow::AddObserver(ChosenObjectRowObserver* observer) {
  observer_list_.AddObserver(observer);
}

ChosenObjectRow::~ChosenObjectRow() {}

void ChosenObjectRow::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  // Change the icon to reflect the selected setting.
  const gfx::Image& image = PageInfoUI::GetChosenObjectIcon(*info_, true);
  icon_->SetImage(image.ToImageSkia());

  DCHECK(delete_button_->visible());
  delete_button_->SetVisible(false);

  for (ChosenObjectRowObserver& observer : observer_list_) {
    observer.OnChosenObjectDeleted(*info_);
  }
}
