// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media_security/multi_profile_media_tray_item.h"

#include "ash/ash_view_ids.h"
#include "ash/media_controller.h"
#include "ash/public/interfaces/media.mojom.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_bubble.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/status_area_widget_test_helper.h"
#include "ash/test/test_shell_delegate.h"
#include "ui/views/bubble/tray_bubble_view.h"

namespace ash {

class MultiProfileMediaTrayItemTest : public test::AshTestBase {
 public:
  MultiProfileMediaTrayItemTest() {}
  ~MultiProfileMediaTrayItemTest() override {}

  void SetMediaCaptureState(mojom::MediaCaptureState state) {
    // Create the fake update.
    SessionController* controller = Shell::Get()->session_controller();
    std::vector<mojom::MediaCaptureState> v;
    for (int i = 0; i < controller->NumberOfLoggedInUsers(); ++i)
      v.push_back(state);
    Shell::Get()->media_controller()->NotifyCaptureState(v);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MultiProfileMediaTrayItemTest);
};

// ash_unittests. still failing.
TEST_F(MultiProfileMediaTrayItemTest, NotifyMediaCaptureChange) {
  TrayItemView::DisableAnimationsForTest();
  GetSessionControllerClient()->CreatePredefinedUserSessions(2);

  SystemTray* system_tray = GetPrimarySystemTray();
  system_tray->ShowDefaultView(BUBBLE_CREATE_NEW);
  views::View* in_user_view =
      system_tray->GetSystemBubble()->bubble_view()->GetViewByID(
          VIEW_ID_USER_VIEW_MEDIA_INDICATOR);

  StatusAreaWidget* widget = StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  EXPECT_TRUE(widget->GetRootView()->visible());
  views::View* tray_view =
      widget->GetRootView()->GetViewByID(VIEW_ID_MEDIA_TRAY_VIEW);

  SetMediaCaptureState(mojom::MediaCaptureState::NONE);
  EXPECT_FALSE(tray_view->visible());
  EXPECT_FALSE(in_user_view->visible());

  SetMediaCaptureState(mojom::MediaCaptureState::AUDIO);
  EXPECT_TRUE(tray_view->visible());
  EXPECT_TRUE(in_user_view->visible());

  SetMediaCaptureState(mojom::MediaCaptureState::AUDIO_VIDEO);
  EXPECT_TRUE(tray_view->visible());
  EXPECT_TRUE(in_user_view->visible());

  SetMediaCaptureState(mojom::MediaCaptureState::NONE);
  EXPECT_FALSE(tray_view->visible());
  EXPECT_FALSE(in_user_view->visible());
}

}  // namespace ash
