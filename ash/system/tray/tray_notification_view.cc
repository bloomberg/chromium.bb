// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_notification_view.h"

#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_constants.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {
namespace internal {

TrayNotificationView::TrayNotificationView(SystemTrayItem* owner, int icon_id)
    : owner_(owner),
      icon_id_(icon_id),
      icon_(NULL) {
}

TrayNotificationView::~TrayNotificationView() {
}

void TrayNotificationView::InitView(views::View* contents) {
  set_background(views::Background::CreateSolidBackground(kBackgroundColor));

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  views::ImageButton* close_button = new views::ImageButton(this);
  close_button->SetImage(views::CustomButton::STATE_NORMAL,
                         ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                             IDR_MESSAGE_CLOSE));
  close_button->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                  views::ImageButton::ALIGN_MIDDLE);

  icon_ = new views::ImageView;
  if (icon_id_ != 0) {
    icon_->SetImage(
        ResourceBundle::GetSharedInstance().GetImageSkiaNamed(icon_id_));
  }

  views::ColumnSet* columns = layout->AddColumnSet(0);

  columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal / 2);

  // Icon
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                     0, /* resize percent */
                     views::GridLayout::FIXED,
                     kNotificationIconWidth, kNotificationIconWidth);

  columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal / 2);

  // Contents
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     100, /* resize percent */
                     views::GridLayout::FIXED,
                     kTrayNotificationContentsWidth,
                     kTrayNotificationContentsWidth);

  columns->AddPaddingColumn(0, kTrayPopupPaddingHorizontal / 2);

  // Close button
  columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                     0, /* resize percent */
                     views::GridLayout::FIXED,
                     kNotificationButtonWidth, kNotificationButtonWidth);

  // Layout rows
  layout->AddPaddingRow(0, kTrayPopupPaddingBetweenItems);
  layout->StartRow(0, 0);
  layout->AddView(icon_);
  layout->AddView(contents);
  layout->AddView(close_button);
  layout->AddPaddingRow(0, kTrayPopupPaddingBetweenItems);
}

void TrayNotificationView::SetIconImage(const gfx::ImageSkia& image) {
  icon_->SetImage(image);
  SchedulePaint();
}

const gfx::ImageSkia& TrayNotificationView::GetIconImage() const {
  return icon_->GetImage();
}

void TrayNotificationView::UpdateView(views::View* new_contents) {
  RemoveAllChildViews(true);
  InitView(new_contents);
  Layout();
  PreferredSizeChanged();
  SchedulePaint();
}

void TrayNotificationView::UpdateViewAndImage(views::View* new_contents,
                                              const gfx::ImageSkia& image) {
  RemoveAllChildViews(true);
  InitView(new_contents);
  icon_->SetImage(image);
  Layout();
  PreferredSizeChanged();
  SchedulePaint();
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

void TrayNotificationView::OnClose() {
}

void TrayNotificationView::OnClickAction() {
}

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

}  // namespace internal
}  // namespace ash
