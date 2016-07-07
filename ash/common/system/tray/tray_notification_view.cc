// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_notification_view.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "ash/common/system/tray/tray_constants.h"
#include "grit/ash_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {

namespace {

// Maps a non-MD PNG resource id to its corresponding MD vector icon id.
// TODO(tdanderson): Remove this once material design is enabled by
// default. See crbug.com/614453.
gfx::VectorIconId ResourceIdToVectorIconId(int resource_id) {
  gfx::VectorIconId vector_id = gfx::VectorIconId::VECTOR_ICON_NONE;
  switch (resource_id) {
    case IDR_AURA_UBER_TRAY_ACCESSIBILITY_DARK:
      return gfx::VectorIconId::SYSTEM_MENU_ACCESSIBILITY;
    case IDR_AURA_UBER_TRAY_SMS:
      return gfx::VectorIconId::SYSTEM_MENU_SMS;
    default:
      NOTREACHED();
      break;
  }

  return vector_id;
}

}  // namespace

TrayNotificationView::TrayNotificationView(SystemTrayItem* owner, int icon_id)
    : owner_(owner), icon_id_(icon_id), icon_(NULL), autoclose_delay_(0) {}

TrayNotificationView::~TrayNotificationView() {}

void TrayNotificationView::InitView(views::View* contents) {
  set_background(views::Background::CreateSolidBackground(kBackgroundColor));

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  views::ImageButton* close_button = new views::ImageButton(this);
  close_button->SetImage(
      views::CustomButton::STATE_NORMAL,
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(IDR_MESSAGE_CLOSE));
  close_button->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                  views::ImageButton::ALIGN_MIDDLE);

  icon_ = new views::ImageView;
  if (icon_id_ != 0) {
    if (MaterialDesignController::UseMaterialDesignSystemIcons()) {
      icon_->SetImage(CreateVectorIcon(ResourceIdToVectorIconId(icon_id_),
                                       kMenuIconSize, kMenuIconColor));
    } else {
      icon_->SetImage(
          ResourceBundle::GetSharedInstance().GetImageSkiaNamed(icon_id_));
    }
  }

  views::ColumnSet* columns = layout->AddColumnSet(0);

  columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal / 2);

  // Icon
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                     0, /* resize percent */
                     views::GridLayout::FIXED, kNotificationIconWidth,
                     kNotificationIconWidth);

  columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal / 2);

  // Contents
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     100, /* resize percent */
                     views::GridLayout::FIXED, kTrayNotificationContentsWidth,
                     kTrayNotificationContentsWidth);

  columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal / 2);

  // Close button
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                     0, /* resize percent */
                     views::GridLayout::FIXED, kNotificationButtonWidth,
                     kNotificationButtonWidth);

  // Layout rows
  layout->AddPaddingRow(0, kTrayPopupPaddingBetweenItems);
  layout->StartRow(0, 0);
  layout->AddView(icon_);
  layout->AddView(contents);
  layout->AddView(close_button);
  layout->AddPaddingRow(0, kTrayPopupPaddingBetweenItems);
}

void TrayNotificationView::UpdateView(views::View* new_contents) {
  RemoveAllChildViews(true);
  InitView(new_contents);
  Layout();
  PreferredSizeChanged();
  SchedulePaint();
}

void TrayNotificationView::StartAutoCloseTimer(int seconds) {
  autoclose_.Stop();
  autoclose_delay_ = seconds;
  if (autoclose_delay_) {
    autoclose_.Start(FROM_HERE, base::TimeDelta::FromSeconds(autoclose_delay_),
                     this, &TrayNotificationView::HandleClose);
  }
}

void TrayNotificationView::StopAutoCloseTimer() {
  autoclose_.Stop();
}

void TrayNotificationView::RestartAutoCloseTimer() {
  if (autoclose_delay_)
    StartAutoCloseTimer(autoclose_delay_);
}

void TrayNotificationView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  HandleClose();
}

bool TrayNotificationView::OnMousePressed(const ui::MouseEvent& event) {
  HandleClickAction();
  return true;
}

void TrayNotificationView::OnGestureEvent(ui::GestureEvent* event) {
  SlideOutView::OnGestureEvent(event);
  if (event->handled())
    return;
  if (event->type() != ui::ET_GESTURE_TAP)
    return;
  HandleClickAction();
  event->SetHandled();
}

void TrayNotificationView::OnClose() {}

void TrayNotificationView::OnClickAction() {}

void TrayNotificationView::OnSlideOut() {
  owner_->HideNotificationView();
}

void TrayNotificationView::HandleClose() {
  OnClose();
  owner_->HideNotificationView();
}

void TrayNotificationView::HandleClickAction() {
  HandleClose();
  OnClickAction();
}

}  // namespace ash
