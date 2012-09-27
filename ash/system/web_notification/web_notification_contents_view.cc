// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_contents_view.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "ash/wm/shelf_types.h"

namespace ash {

namespace message_center {

WebNotificationContentsView::WebNotificationContentsView(
    WebNotificationTray* tray) : tray_(tray) {
  // TODO(stevenjb): Remove this border when TrayBubbleBorder is integrated
  // with BubbleBorder.
  int left = (tray->shelf_alignment() == SHELF_ALIGNMENT_LEFT) ? 0 : 1;
  int right = (tray->shelf_alignment() == SHELF_ALIGNMENT_RIGHT) ? 0 : 1;
  int bottom = (tray->shelf_alignment() == SHELF_ALIGNMENT_BOTTOM) ? 0 : 1;
  set_border(views::Border::CreateSolidSidedBorder(
      1, left, bottom, right, ash::kBorderDarkColor));
  set_notify_enter_exit_on_child(true);
}

WebNotificationContentsView::~WebNotificationContentsView() {}

}  // namespace message_center

}  // namespace ash
