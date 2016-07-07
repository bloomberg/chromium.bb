// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/media_security/multi_profile_media_tray_item.h"

#include "ash/common/ash_view_ids.h"
#include "ash/common/system/tray/system_tray_bubble.h"
#include "ash/common/system/tray/tray_item_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/status_area_widget_test_helper.h"
#include "ash/test/test_session_state_delegate.h"
#include "ash/test/test_shell_delegate.h"
#include "ui/views/bubble/tray_bubble_view.h"

namespace ash {

typedef test::AshTestBase MultiProfileMediaTrayItemTest;

TEST_F(MultiProfileMediaTrayItemTest, NotifyMediaCaptureChange) {
  TrayItemView::DisableAnimationsForTest();
  test::TestShellDelegate* shell_delegate =
      ash_test_helper()->test_shell_delegate();
  test::TestSessionStateDelegate* session_state_delegate =
      test::AshTestHelper::GetTestSessionStateDelegate();
  session_state_delegate->set_logged_in_users(2);

  SystemTray* system_tray = GetPrimarySystemTray();
  system_tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  views::View* in_user_view =
      system_tray->GetSystemBubble()->bubble_view()->GetViewByID(
          VIEW_ID_USER_VIEW_MEDIA_INDICATOR);

  StatusAreaWidget* widget = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  EXPECT_TRUE(widget->GetRootView()->visible());
  views::View* tray_view =
      widget->GetRootView()->GetViewByID(VIEW_ID_MEDIA_TRAY_VIEW);

  EXPECT_FALSE(tray_view->visible());
  EXPECT_FALSE(in_user_view->visible());

  shell_delegate->SetMediaCaptureState(MEDIA_CAPTURE_AUDIO);
  EXPECT_TRUE(tray_view->visible());
  EXPECT_TRUE(in_user_view->visible());

  shell_delegate->SetMediaCaptureState(MEDIA_CAPTURE_AUDIO_VIDEO);
  EXPECT_TRUE(tray_view->visible());
  EXPECT_TRUE(in_user_view->visible());

  shell_delegate->SetMediaCaptureState(MEDIA_CAPTURE_NONE);
  EXPECT_FALSE(tray_view->visible());
  EXPECT_FALSE(in_user_view->visible());
}

}  // namespace ash
