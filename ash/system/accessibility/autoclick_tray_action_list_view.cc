// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/autoclick_tray_action_list_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/autoclick_tray.h"
#include "ash/system/accessibility/autoclick_tray_action_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Used for testing.
const int kLeftClickViewID = 2;
const int kRightClickViewID = 3;
const int kDoubleClickViewID = 4;
const int kDragAndDropViewID = 5;
const int kPauseViewID = 6;

}  // namespace

AutoclickTrayActionListView::AutoclickTrayActionListView(
    AutoclickTray* tray,
    mojom::AutoclickEventType selected_event_type)
    : autoclick_tray_(tray), selected_event_type_(selected_event_type) {
  DCHECK(tray);
  views::BoxLayout* layout = SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));

  // The views will be owned by the view hierarchy.
  views::View* left_click_view = new AutoclickTrayActionView(
      this, mojom::AutoclickEventType::kLeftClick,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUTOCLICK_OPTION_LEFT_CLICK),
      selected_event_type_ == mojom::AutoclickEventType::kLeftClick);
  left_click_view->set_id(kLeftClickViewID);
  AddChildView(left_click_view);
  layout->SetFlexForView(left_click_view, 1);

  views::View* right_click_view = new AutoclickTrayActionView(
      this, mojom::AutoclickEventType::kRightClick,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUTOCLICK_OPTION_RIGHT_CLICK),
      selected_event_type_ == mojom::AutoclickEventType::kRightClick);
  right_click_view->set_id(kRightClickViewID);
  AddChildView(right_click_view);
  layout->SetFlexForView(right_click_view, 1);

  views::View* double_click_view = new AutoclickTrayActionView(
      this, mojom::AutoclickEventType::kDoubleClick,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUTOCLICK_OPTION_DOUBLE_CLICK),
      selected_event_type_ == mojom::AutoclickEventType::kDoubleClick);
  double_click_view->set_id(kDoubleClickViewID);
  AddChildView(double_click_view);
  layout->SetFlexForView(double_click_view, 1);

  views::View* drag_and_drop_view = new AutoclickTrayActionView(
      this, mojom::AutoclickEventType::kDragAndDrop,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUTOCLICK_OPTION_DRAG_AND_DROP),
      selected_event_type_ == mojom::AutoclickEventType::kDragAndDrop);
  drag_and_drop_view->set_id(kDragAndDropViewID);
  AddChildView(drag_and_drop_view);
  layout->SetFlexForView(drag_and_drop_view, 1);

  views::View* pause_view = new AutoclickTrayActionView(
      this, mojom::AutoclickEventType::kNoAction,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUTOCLICK_OPTION_NO_ACTION),
      selected_event_type_ == mojom::AutoclickEventType::kNoAction);
  pause_view->set_id(kPauseViewID);
  AddChildView(pause_view);
  layout->SetFlexForView(pause_view, 1);
}

void AutoclickTrayActionListView::PerformTrayActionViewAction(
    mojom::AutoclickEventType type) {
  autoclick_tray_->OnEventTypePressed(type);
}

}  // namespace ash
