// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/autoclick_tray_action_view.h"

#include "ash/system/accessibility/autoclick_tray_action_list_view.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

AutoclickTrayActionView::AutoclickTrayActionView(
    AutoclickTrayActionListView* list_view,
    mojom::AutoclickEventType event_type,
    const base::string16& label,
    bool selected)
    : ActionableView(TrayPopupInkDropStyle::FILL_BOUNDS),
      list_view_(list_view),
      event_type_(event_type),
      selected_(selected) {
  SetInkDropMode(InkDropMode::ON);

  TriView* tri_view = TrayPopupUtils::CreateDefaultRowView();
  AddChildView(tri_view);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  views::ImageView* icon = new views::ImageView();
  // TODO(katie): Use autoclick assets when available.
  icon->SetImage(gfx::CreateVectorIcon(kSystemTraySelectToSpeakNewuiIcon,
                                       gfx::kGoogleGrey700));
  tri_view->AddView(TriView::Container::START, icon);

  auto* label_view = TrayPopupUtils::CreateDefaultLabel();
  label_view->SetText(label);
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL,
                           false /* use unified theme */);
  style.SetupLabel(label_view);

  label_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  tri_view->AddView(TriView::Container::CENTER, label_view);

  if (selected) {
    // The checked button indicates the autoclick type is selected.
    views::ImageView* checked_image = TrayPopupUtils::CreateMainImageView();
    checked_image->SetImage(gfx::CreateVectorIcon(
        kCheckCircleIcon, kMenuIconSize, gfx::kGoogleGreen700));
    tri_view->AddView(TriView::Container::END, checked_image);
  }
  SetAccessibleName(label_view->text());
}

bool AutoclickTrayActionView::PerformAction(const ui::Event& event) {
  list_view_->PerformTrayActionViewAction(event_type_);
  return true;
}

void AutoclickTrayActionView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ActionableView::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kCheckBox;
  node_data->SetCheckedState(selected_ ? ax::mojom::CheckedState::kTrue
                                       : ax::mojom::CheckedState::kFalse);
}

}  // namespace ash
