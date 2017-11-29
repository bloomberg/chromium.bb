// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/chosen_object_view.h"

#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/views/page_info/chosen_object_view_observer.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"

ChosenObjectView::ChosenObjectView(
    std::unique_ptr<PageInfoUI::ChosenObjectInfo> info)
    : info_(std::move(info)) {
  // |ChosenObjectView| layout (fills parent):
  // *------------------------------------*
  // | Icon | Chosen Object Name      | X |
  // *------------------------------------*
  //
  // Where the icon and close button columns are fixed widths.

  constexpr float kFixed = 0.f;
  constexpr float kStretchy = 1.f;
  views::GridLayout* layout = views::GridLayout::CreateAndInstall(this);
  const int column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                        kFixed, views::GridLayout::FIXED,
                        PageInfoBubbleView::kIconColumnWidth, 0);
  column_set->AddPaddingColumn(kFixed,
                               ChromeLayoutProvider::Get()->GetDistanceMetric(
                                   views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                        kStretchy, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                        kFixed, views::GridLayout::USE_PREF,
                        PageInfoBubbleView::kIconColumnWidth, 0);

  layout->StartRow(kStretchy, column_set_id);
  // Create the chosen object icon.
  icon_ = new views::ImageView();
  const gfx::Image& image = PageInfoUI::GetChosenObjectIcon(*info_, false);
  icon_->SetImage(image.ToImageSkia());
  layout->AddView(icon_);
  // Create the label that displays the chosen object name.
  views::Label* label = new views::Label(
      l10n_util::GetStringFUTF16(info_->ui_info.label_string_id,
                                 PageInfoUI::ChosenObjectToUIString(*info_)),
      CONTEXT_BODY_TEXT_LARGE);
  layout->AddView(label);
  // Create the delete button.
  if (ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    delete_button_ = views::CreateVectorImageButton(this);
    views::SetImageFromVectorIcon(
        delete_button_, vector_icons::kClose16Icon,
        views::style::GetColor(*this, CONTEXT_BODY_TEXT_LARGE,
                               views::style::STYLE_PRIMARY));

  } else {
    delete_button_ = new views::ImageButton(this);
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    delete_button_->SetImage(views::ImageButton::STATE_NORMAL,
                             rb.GetImageSkiaNamed(IDR_CLOSE_2));
    delete_button_->SetImage(views::ImageButton::STATE_HOVERED,
                             rb.GetImageSkiaNamed(IDR_CLOSE_2_H));
    delete_button_->SetImage(views::ImageButton::STATE_PRESSED,
                             rb.GetImageSkiaNamed(IDR_CLOSE_2_P));
  }
  delete_button_->SetFocusForPlatform();
  delete_button_->set_request_focus_on_press(true);
  delete_button_->SetTooltipText(
      l10n_util::GetStringUTF16(info_->ui_info.delete_tooltip_string_id));
  layout->AddView(delete_button_);
}

void ChosenObjectView::AddObserver(ChosenObjectViewObserver* observer) {
  observer_list_.AddObserver(observer);
}

ChosenObjectView::~ChosenObjectView() {}

void ChosenObjectView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  // Change the icon to reflect the selected setting.
  const gfx::Image& image = PageInfoUI::GetChosenObjectIcon(*info_, true);
  icon_->SetImage(image.ToImageSkia());

  DCHECK(delete_button_->visible());
  delete_button_->SetVisible(false);

  for (ChosenObjectViewObserver& observer : observer_list_) {
    observer.OnChosenObjectDeleted(*info_);
  }
}
