// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_view.h"

#include <memory>

#include "ash/media/media_notification_constants.h"
#include "ash/media/media_notification_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/message_center/views/notification_control_buttons_view.h"

namespace ash {

using media_session::test::TestMediaController;

namespace {

// The icons size is 32 and INSETS_VECTOR_IMAGE_BUTTON will add padding around
// the image.
const int kMediaButtonIconSize = 40;

}  // namespace

class MediaNotificationViewTest : public AshTestBase {
 public:
  MediaNotificationViewTest() = default;
  ~MediaNotificationViewTest() override = default;

  // AshTestBase
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kMediaSessionNotification);

    AshTestBase::SetUp();

    media_controller_ = std::make_unique<TestMediaController>();
    Shell::Get()->media_notification_controller()->SetMediaControllerForTesting(
        media_controller_->CreateMediaControllerPtr());

    // Set a custom view factory to create and capture the notification view.
    message_center::MessageViewFactory::
        ClearCustomNotificationViewFactoryForTest(
            kMediaSessionNotificationCustomViewType);
    message_center::MessageViewFactory::SetCustomNotificationViewFactory(
        kMediaSessionNotificationCustomViewType,
        base::BindRepeating(
            &MediaNotificationViewTest::CreateAndCaptureCustomView,
            base::Unretained(this)));

    // Show the notification.
    Shell::Get()->media_notification_controller()->OnFocusGained(
        media_session::mojom::MediaSessionInfo::New(),
        media_session::mojom::AudioFocusType::kGain);

    message_center::Notification* notification =
        message_center::MessageCenter::Get()->FindVisibleNotificationById(
            kMediaSessionNotificationId);
    ASSERT_TRUE(notification);

    // Open the system tray. This will trigger the view to be created.
    auto* unified_system_tray =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()
            ->unified_system_tray();
    unified_system_tray->ShowBubble(false /* show_by_click */);
    unified_system_tray->ActivateBubble();

    // Check that the view was captured.
    ASSERT_TRUE(view_);
    view_->set_notify_enter_exit_on_child(true);
  }

  void TearDown() override {
    view_ = nullptr;

    message_center::MessageViewFactory::
        ClearCustomNotificationViewFactoryForTest(
            kMediaSessionNotificationCustomViewType);

    AshTestBase::TearDown();
  }

  bool IsControlButtonsViewVisible() const {
    return view()->GetControlButtonsView()->layer()->opacity() > 0;
  }

  MediaNotificationView* view() const { return view_; }

  TestMediaController* media_controller() const {
    return media_controller_.get();
  }

  views::View* button_row() const { return view_->button_row_; }

 private:
  std::unique_ptr<message_center::MessageView> CreateAndCaptureCustomView(
      const message_center::Notification& notification) {
    auto view = std::make_unique<MediaNotificationView>(notification);
    view_ = view.get();
    return view;
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestMediaController> media_controller_;
  std::unique_ptr<views::Widget> widget_;
  MediaNotificationView* view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationViewTest);
};

TEST_F(MediaNotificationViewTest, ShowControlsOnHover) {
  EXPECT_FALSE(IsControlButtonsViewVisible());

  {
    gfx::Point cursor_location(1, 1);
    views::View::ConvertPointToScreen(view(), &cursor_location);
    GetEventGenerator()->MoveMouseTo(cursor_location);
    GetEventGenerator()->SendMouseEnter();
  }

  EXPECT_TRUE(IsControlButtonsViewVisible());

  {
    gfx::Point cursor_location(-1, -1);
    views::View::ConvertPointToScreen(view(), &cursor_location);
    GetEventGenerator()->MoveMouseTo(cursor_location.x(), cursor_location.y());
    GetEventGenerator()->SendMouseExit();
  }

  EXPECT_FALSE(IsControlButtonsViewVisible());
}

TEST_F(MediaNotificationViewTest, ButtonsSanityCheck) {
  EXPECT_TRUE(button_row()->visible());
  EXPECT_GT(button_row()->width(), 0);
  EXPECT_GT(button_row()->height(), 0);

  EXPECT_EQ(3, button_row()->child_count());

  for (int i = 0; i < button_row()->child_count(); ++i) {
    const views::Button* child =
        views::Button::AsButton(button_row()->child_at(i));

    EXPECT_TRUE(child->visible());
    EXPECT_EQ(kMediaButtonIconSize, child->width());
    EXPECT_EQ(kMediaButtonIconSize, child->height());
  }
}

TEST_F(MediaNotificationViewTest, NextTrackButtonClick) {
  EXPECT_EQ(0, media_controller()->next_track_count());

  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToScreen(button_row()->child_at(2),
                                    &cursor_location);
  GetEventGenerator()->MoveMouseTo(cursor_location.x(), cursor_location.y());
  GetEventGenerator()->ClickLeftButton();
  Shell::Get()->media_notification_controller()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->next_track_count());
}

TEST_F(MediaNotificationViewTest, PlayPauseButtonClick) {
  EXPECT_EQ(0, media_controller()->toggle_suspend_resume_count());

  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToScreen(button_row()->child_at(1),
                                    &cursor_location);
  GetEventGenerator()->MoveMouseTo(cursor_location.x(), cursor_location.y());
  GetEventGenerator()->ClickLeftButton();
  Shell::Get()->media_notification_controller()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->toggle_suspend_resume_count());
}

TEST_F(MediaNotificationViewTest, PreviousTrackButtonClick) {
  EXPECT_EQ(0, media_controller()->previous_track_count());

  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToScreen(button_row()->child_at(0),
                                    &cursor_location);
  GetEventGenerator()->MoveMouseTo(cursor_location.x(), cursor_location.y());
  GetEventGenerator()->ClickLeftButton();
  Shell::Get()->media_notification_controller()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->previous_track_count());
}

TEST_F(MediaNotificationViewTest, ClickNotification) {
  EXPECT_EQ(0, media_controller()->toggle_suspend_resume_count());

  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToScreen(view(), &cursor_location);
  GetEventGenerator()->MoveMouseTo(cursor_location.x(), cursor_location.y());
  GetEventGenerator()->ClickLeftButton();
  Shell::Get()->media_notification_controller()->FlushForTesting();

  EXPECT_EQ(0, media_controller()->toggle_suspend_resume_count());
}

}  // namespace ash
