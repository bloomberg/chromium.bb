// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_notification_view.h"

#include "ash/common/system/tray/tray_constants.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {

namespace {

// Maps a non-MD PNG resource id to its corresponding MD vector icon.
// TODO(tdanderson): Remove this once material design is enabled by
// default. See crbug.com/614453.
const gfx::VectorIcon& ResourceIdToVectorIcon(int resource_id) {
  switch (resource_id) {
    case IDR_AURA_UBER_TRAY_ACCESSIBILITY_DARK:
      return kSystemMenuAccessibilityIcon;
    default:
      NOTREACHED();
      break;
  }

  return gfx::kNoneIcon;
}

}  // namespace

TrayNotificationView::TrayNotificationView(int icon_id)
    : icon_id_(icon_id), icon_(NULL) {}

TrayNotificationView::~TrayNotificationView() {}

void TrayNotificationView::InitView(views::View* contents) {
  set_background(views::Background::CreateSolidBackground(kBackgroundColor));

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  views::ImageView* close_button = new views::ImageView();
  close_button->SetImage(
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(IDR_MESSAGE_CLOSE));
  close_button->SetHorizontalAlignment(views::ImageView::CENTER);
  close_button->SetVerticalAlignment(views::ImageView::CENTER);

  icon_ = new views::ImageView;
  if (icon_id_ != 0) {
    icon_->SetImage(gfx::CreateVectorIcon(ResourceIdToVectorIcon(icon_id_),
                                          kMenuIconColor));
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

}  // namespace ash
