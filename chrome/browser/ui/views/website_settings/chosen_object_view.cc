// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/chosen_object_view.h"

#include "chrome/browser/ui/views/website_settings/chosen_object_view_observer.h"
#include "chrome/browser/ui/views/website_settings/website_settings_popup_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"

ChosenObjectView::ChosenObjectView(
    std::unique_ptr<WebsiteSettingsUI::ChosenObjectInfo> info)
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
  const gfx::Image& image =
      WebsiteSettingsUI::GetChosenObjectIcon(*info_, false);
  icon_->SetImage(image.ToImageSkia());
  layout->AddView(icon_, 1, 1, views::GridLayout::CENTER,
                  views::GridLayout::CENTER);
  // Create the label that displays the permission type.
  views::Label* label = new views::Label(l10n_util::GetStringFUTF16(
      info_->ui_info.label_string_id,
      WebsiteSettingsUI::ChosenObjectToUIString(*info_)));
  layout->AddView(label, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::CENTER);
  // Create the delete button.
  delete_button_ = new views::ImageButton(this);
  delete_button_->SetFocusBehavior(FocusBehavior::ALWAYS);
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

void ChosenObjectView::AddObserver(ChosenObjectViewObserver* observer) {
  observer_list_.AddObserver(observer);
}

ChosenObjectView::~ChosenObjectView() {}

void ChosenObjectView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  // Change the icon to reflect the selected setting.
  const gfx::Image& image =
      WebsiteSettingsUI::GetChosenObjectIcon(*info_, true);
  icon_->SetImage(image.ToImageSkia());

  RemoveChildView(delete_button_);
  delete delete_button_;
  delete_button_ = nullptr;

  FOR_EACH_OBSERVER(ChosenObjectViewObserver, observer_list_,
                    OnChosenObjectDeleted(*info_));
}
