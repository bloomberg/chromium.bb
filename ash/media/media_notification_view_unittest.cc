// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_view.h"

#include <memory>

#include "ash/media/media_notification_background.h"
#include "ash/media/media_notification_constants.h"
#include "ash/media/media_notification_controller.h"
#include "ash/media/media_notification_item.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/views/controls/image_view.h"

namespace ash {

using media_session::mojom::MediaSessionAction;
using media_session::test::TestMediaController;

namespace {

// The icons size is 24 and INSETS_VECTOR_IMAGE_BUTTON will add padding around
// the image.
const int kMediaButtonIconSize = 24;

const char kTestAppName[] = "app name";

// Checks if the view class name is used by a media button.
bool IsMediaButtonType(const char* class_name) {
  return class_name == views::ImageButton::kViewClassName ||
         class_name == views::ToggleImageButton::kViewClassName;
}

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

    request_id_ = base::UnguessableToken::Create();

    ShowNotificationAndCaptureView(
        media_session::mojom::MediaSessionInfo::New());
  }

  void ShowNotificationAndCaptureView(
      media_session::mojom::MediaSessionInfoPtr session_info) {
    view_ = nullptr;

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
    media_session::mojom::AudioFocusRequestStatePtr session(
        media_session::mojom::AudioFocusRequestState::New());
    session->session_info = std::move(session_info);
    session->session_info->is_controllable = true;
    session->request_id = request_id_;
    Shell::Get()->media_notification_controller()->OnFocusGained(
        std::move(session));

    message_center::Notification* notification =
        message_center::MessageCenter::Get()->FindVisibleNotificationById(
            request_id_.ToString());
    ASSERT_TRUE(notification);

    // Open the system tray. This will trigger the view to be created.
    auto* unified_system_tray =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()
            ->unified_system_tray();
    unified_system_tray->SetTrayEnabled(true);
    unified_system_tray->ShowBubble(false /* show_by_click */);
    unified_system_tray->ActivateBubble();

    // Check that the view was captured.
    ASSERT_TRUE(view_);
    view_->set_notify_enter_exit_on_child(true);

    // Inject the test media controller into the item.
    ASSERT_TRUE(GetItem());
    media_controller_ = std::make_unique<TestMediaController>();
    GetItem()->SetMediaControllerForTesting(
        media_controller_->CreateMediaControllerPtr());
  }

  void TearDown() override {
    view_ = nullptr;

    actions_.clear();

    message_center::MessageViewFactory::
        ClearCustomNotificationViewFactoryForTest(
            kMediaSessionNotificationCustomViewType);

    AshTestBase::TearDown();
  }

  bool IsControlButtonsViewVisible() const {
    return view()->GetControlButtonsView()->layer()->opacity() > 0;
  }

  void UpdateWithSampleMetadata() {
    EXPECT_FALSE(title_artist_row()->visible());
    EXPECT_FALSE(title_label()->visible());
    EXPECT_FALSE(artist_label()->visible());

    media_session::MediaMetadata metadata;
    metadata.title = base::ASCIIToUTF16("title");
    metadata.artist = base::ASCIIToUTF16("artist");

    GetItem()->MediaSessionMetadataChanged(metadata);

    EXPECT_TRUE(title_artist_row()->visible());
    EXPECT_TRUE(title_label()->visible());
    EXPECT_TRUE(artist_label()->visible());

    EXPECT_EQ(metadata.title, title_label()->text());
    EXPECT_EQ(metadata.artist, artist_label()->text());
  }

  void EnableAllActions() {
    actions_.insert(MediaSessionAction::kPlay);
    actions_.insert(MediaSessionAction::kPause);
    actions_.insert(MediaSessionAction::kPreviousTrack);
    actions_.insert(MediaSessionAction::kNextTrack);
    actions_.insert(MediaSessionAction::kSeekBackward);
    actions_.insert(MediaSessionAction::kSeekForward);
    actions_.insert(MediaSessionAction::kStop);

    NotifyUpdatedActions();
  }

  void EnableAction(MediaSessionAction action) {
    actions_.insert(action);
    NotifyUpdatedActions();
  }

  void DisableAction(MediaSessionAction action) {
    actions_.erase(action);
    NotifyUpdatedActions();
  }

  MediaNotificationView* view() const { return view_; }

  TestMediaController* media_controller() const {
    return media_controller_.get();
  }

  message_center::NotificationHeaderView* header_row() const {
    return view_->header_row_;
  }

  views::View* button_row() const { return view_->button_row_; }

  views::View* title_artist_row() const { return view_->title_artist_row_; }

  views::Label* title_label() const { return view_->title_label_; }

  views::Label* artist_label() const { return view_->artist_label_; }

  views::Button* GetButtonForAction(MediaSessionAction action) const {
    for (int i = 0; i < button_row()->child_count(); ++i) {
      views::Button* child = views::Button::AsButton(button_row()->child_at(i));

      if (child->tag() == static_cast<int>(action))
        return child;
    }

    return nullptr;
  }

  bool IsActionButtonVisible(MediaSessionAction action) const {
    return GetButtonForAction(action)->visible();
  }

  MediaNotificationItem* GetItem() const {
    return Shell::Get()->media_notification_controller()->GetItem(
        request_id_.ToString());
  }

  bool is_expanded() const { return view_->expanded_; }

  const base::UnguessableToken& request_id() const { return request_id_; }

  const gfx::ImageSkia& GetArtworkImage() const {
    return view_->GetMediaNotificationBackground()->artwork_;
  }

 private:
  std::unique_ptr<message_center::MessageView> CreateAndCaptureCustomView(
      const message_center::Notification& notification) {
    auto view = std::make_unique<MediaNotificationView>(notification);
    view_ = view.get();
    return view;
  }

  void NotifyUpdatedActions() {
    GetItem()->MediaSessionActionsChanged(
        std::vector<MediaSessionAction>(actions_.begin(), actions_.end()));
  }

  base::UnguessableToken request_id_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::set<MediaSessionAction> actions_;

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
    GetEventGenerator()->MoveMouseTo(cursor_location);
    GetEventGenerator()->SendMouseExit();
  }

  EXPECT_FALSE(IsControlButtonsViewVisible());
}

TEST_F(MediaNotificationViewTest, ButtonsSanityCheck) {
  EnableAllActions();

  EXPECT_TRUE(button_row()->visible());
  EXPECT_GT(button_row()->width(), 0);
  EXPECT_GT(button_row()->height(), 0);

  EXPECT_EQ(5, button_row()->child_count());

  for (int i = 0; i < button_row()->child_count(); ++i) {
    const views::Button* child =
        views::Button::AsButton(button_row()->child_at(i));
    ASSERT_TRUE(IsMediaButtonType(child->GetClassName()));

    EXPECT_TRUE(child->visible());
    EXPECT_LT(kMediaButtonIconSize, child->width());
    EXPECT_LT(kMediaButtonIconSize, child->height());
  }

  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kSeekBackward));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kSeekForward));

  // |kPause| cannot be present if |kPlay| is.
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
}

TEST_F(MediaNotificationViewTest, NextTrackButtonClick) {
  EnableAction(MediaSessionAction::kNextTrack);

  EXPECT_EQ(0, media_controller()->next_track_count());

  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToScreen(
      GetButtonForAction(MediaSessionAction::kNextTrack), &cursor_location);
  GetEventGenerator()->MoveMouseTo(cursor_location);
  GetEventGenerator()->ClickLeftButton();
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->next_track_count());
}

TEST_F(MediaNotificationViewTest, PlayButtonClick) {
  EnableAction(MediaSessionAction::kPlay);

  EXPECT_EQ(0, media_controller()->resume_count());

  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToScreen(
      GetButtonForAction(MediaSessionAction::kPlay), &cursor_location);
  GetEventGenerator()->MoveMouseTo(cursor_location);
  GetEventGenerator()->ClickLeftButton();
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->resume_count());
}

TEST_F(MediaNotificationViewTest, PauseButtonClick) {
  EnableAction(MediaSessionAction::kPause);

  EXPECT_EQ(0, media_controller()->suspend_count());

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  GetItem()->MediaSessionInfoChanged(session_info.Clone());

  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToScreen(
      GetButtonForAction(MediaSessionAction::kPause), &cursor_location);
  GetEventGenerator()->MoveMouseTo(cursor_location);
  GetEventGenerator()->ClickLeftButton();
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->suspend_count());
}

TEST_F(MediaNotificationViewTest, PreviousTrackButtonClick) {
  EnableAction(MediaSessionAction::kPreviousTrack);

  EXPECT_EQ(0, media_controller()->previous_track_count());

  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToScreen(
      GetButtonForAction(MediaSessionAction::kPreviousTrack), &cursor_location);
  GetEventGenerator()->MoveMouseTo(cursor_location);
  GetEventGenerator()->ClickLeftButton();
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->previous_track_count());
}

TEST_F(MediaNotificationViewTest, SeekBackwardButtonClick) {
  EnableAction(MediaSessionAction::kSeekBackward);

  EXPECT_EQ(0, media_controller()->seek_backward_count());

  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToScreen(
      GetButtonForAction(MediaSessionAction::kSeekBackward), &cursor_location);
  GetEventGenerator()->MoveMouseTo(cursor_location);
  GetEventGenerator()->ClickLeftButton();
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->seek_backward_count());
}

TEST_F(MediaNotificationViewTest, SeekForwardButtonClick) {
  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_EQ(0, media_controller()->seek_forward_count());

  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToScreen(
      GetButtonForAction(MediaSessionAction::kSeekForward), &cursor_location);
  GetEventGenerator()->MoveMouseTo(cursor_location);
  GetEventGenerator()->ClickLeftButton();
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->seek_forward_count());
}

TEST_F(MediaNotificationViewTest, ClickNotification) {
  EXPECT_EQ(0, media_controller()->toggle_suspend_resume_count());

  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToScreen(view(), &cursor_location);
  GetEventGenerator()->MoveMouseTo(cursor_location);
  GetEventGenerator()->ClickLeftButton();
  GetItem()->FlushForTesting();

  EXPECT_EQ(0, media_controller()->toggle_suspend_resume_count());
}

TEST_F(MediaNotificationViewTest, PlayToggle_FromActiveSessionChanged) {
  {
    views::ToggleImageButton* button = static_cast<views::ToggleImageButton*>(
        GetButtonForAction(MediaSessionAction::kPlay));
    ASSERT_EQ(views::ToggleImageButton::kViewClassName, button->GetClassName());
    EXPECT_FALSE(button->toggled_for_testing());
  }

  media_session::mojom::AudioFocusRequestStatePtr session(
      media_session::mojom::AudioFocusRequestState::New());
  session->request_id = request_id();
  Shell::Get()->media_notification_controller()->OnFocusLost(
      std::move(session));

  // Disable the tray and run the loop to make sure that the existing view is
  // destroyed.
  StatusAreaWidgetTestHelper::GetStatusAreaWidget()
      ->unified_system_tray()
      ->SetTrayEnabled(false);
  base::RunLoop().RunUntilIdle();

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;

  ShowNotificationAndCaptureView(std::move(session_info));

  {
    views::ToggleImageButton* button = static_cast<views::ToggleImageButton*>(
        GetButtonForAction(MediaSessionAction::kPause));
    ASSERT_EQ(views::ToggleImageButton::kViewClassName, button->GetClassName());
    EXPECT_TRUE(button->toggled_for_testing());
  }
}

TEST_F(MediaNotificationViewTest, PlayToggle_FromObserver_Empty) {
  EnableAction(MediaSessionAction::kPlay);

  {
    views::ToggleImageButton* button = static_cast<views::ToggleImageButton*>(
        GetButtonForAction(MediaSessionAction::kPlay));
    ASSERT_EQ(views::ToggleImageButton::kViewClassName, button->GetClassName());
    EXPECT_FALSE(button->toggled_for_testing());
  }

  view()->UpdateWithMediaSessionInfo(
      media_session::mojom::MediaSessionInfo::New());

  {
    views::ToggleImageButton* button = static_cast<views::ToggleImageButton*>(
        GetButtonForAction(MediaSessionAction::kPlay));
    ASSERT_EQ(views::ToggleImageButton::kViewClassName, button->GetClassName());
    EXPECT_FALSE(button->toggled_for_testing());
  }
}

TEST_F(MediaNotificationViewTest, PlayToggle_FromObserver_PlaybackState) {
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  {
    views::ToggleImageButton* button = static_cast<views::ToggleImageButton*>(
        GetButtonForAction(MediaSessionAction::kPlay));
    ASSERT_EQ(views::ToggleImageButton::kViewClassName, button->GetClassName());
    EXPECT_FALSE(button->toggled_for_testing());
  }

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());

  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());

  {
    views::ToggleImageButton* button = static_cast<views::ToggleImageButton*>(
        GetButtonForAction(MediaSessionAction::kPause));
    ASSERT_EQ(views::ToggleImageButton::kViewClassName, button->GetClassName());
    EXPECT_TRUE(button->toggled_for_testing());
  }

  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPaused;
  view()->UpdateWithMediaSessionInfo(session_info.Clone());

  {
    views::ToggleImageButton* button = static_cast<views::ToggleImageButton*>(
        GetButtonForAction(MediaSessionAction::kPlay));
    ASSERT_EQ(views::ToggleImageButton::kViewClassName, button->GetClassName());
    EXPECT_FALSE(button->toggled_for_testing());
  }
}

TEST_F(MediaNotificationViewTest, UpdateMetadata_FromObserver) {
  UpdateWithSampleMetadata();
}

TEST_F(MediaNotificationViewTest, UpdateMetadata_FromObserver_Empty) {
  UpdateWithSampleMetadata();

  GetItem()->MediaSessionMetadataChanged(base::nullopt);

  EXPECT_FALSE(title_artist_row()->visible());
}

TEST_F(MediaNotificationViewTest, UpdateMetadata_FromObserver_EmptyString) {
  UpdateWithSampleMetadata();

  GetItem()->MediaSessionMetadataChanged(media_session::MediaMetadata());

  EXPECT_FALSE(title_artist_row()->visible());
}

TEST_F(MediaNotificationViewTest, UpdateMetadata_FromObserver_NoArtist) {
  EXPECT_FALSE(title_artist_row()->visible());
  EXPECT_FALSE(title_label()->visible());
  EXPECT_FALSE(artist_label()->visible());

  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title");

  GetItem()->MediaSessionMetadataChanged(metadata);

  EXPECT_TRUE(title_artist_row()->visible());
  EXPECT_TRUE(title_label()->visible());
  EXPECT_FALSE(artist_label()->visible());

  EXPECT_EQ(metadata.title, title_label()->text());
}

TEST_F(MediaNotificationViewTest, UpdateMetadata_FromObserver_NoTitle) {
  EXPECT_FALSE(title_artist_row()->visible());
  EXPECT_FALSE(title_label()->visible());
  EXPECT_FALSE(artist_label()->visible());

  media_session::MediaMetadata metadata;
  metadata.artist = base::ASCIIToUTF16("artist");

  GetItem()->MediaSessionMetadataChanged(metadata);

  EXPECT_TRUE(title_artist_row()->visible());
  EXPECT_FALSE(title_label()->visible());
  EXPECT_TRUE(artist_label()->visible());

  EXPECT_EQ(metadata.artist, artist_label()->text());
}

TEST_F(MediaNotificationViewTest, UpdateMetadata_AppName) {
  EXPECT_EQ(
      message_center::MessageCenter::Get()->GetSystemNotificationAppName(),
      header_row()->app_name_for_testing());

  media_session::MediaMetadata metadata;
  metadata.source_title = base::ASCIIToUTF16(kTestAppName);

  GetItem()->MediaSessionMetadataChanged(metadata);

  EXPECT_EQ(base::ASCIIToUTF16(kTestAppName),
            header_row()->app_name_for_testing());

  GetItem()->MediaSessionMetadataChanged(media_session::MediaMetadata());

  EXPECT_EQ(
      message_center::MessageCenter::Get()->GetSystemNotificationAppName(),
      header_row()->app_name_for_testing());
}

TEST_F(MediaNotificationViewTest, Buttons_WhenCollapsed) {
  EnableAllActions();

  media_session::MediaMetadata metadata;
  metadata.artist = base::ASCIIToUTF16("artist");

  GetItem()->MediaSessionMetadataChanged(metadata);

  view()->SetExpanded(false);

  EXPECT_FALSE(is_expanded());

  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPlay));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekBackward));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekForward));

  DisableAction(MediaSessionAction::kPreviousTrack);
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));

  EnableAction(MediaSessionAction::kPreviousTrack);
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));

  DisableAction(MediaSessionAction::kSeekForward);
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekForward));

  EnableAction(MediaSessionAction::kSeekForward);
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekForward));
}

TEST_F(MediaNotificationViewTest, Buttons_WhenExpanded) {
  EnableAllActions();

  media_session::MediaMetadata metadata;
  metadata.artist = base::ASCIIToUTF16("artist");

  GetItem()->MediaSessionMetadataChanged(metadata);

  view()->SetExpanded(true);

  EXPECT_TRUE(is_expanded());

  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPlay));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kSeekBackward));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kSeekForward));
}

TEST_F(MediaNotificationViewTest, ClickHeader_ToggleExpand) {
  EXPECT_TRUE(is_expanded());

  {
    gfx::Point cursor_location(1, 1);
    views::View::ConvertPointToScreen(header_row(), &cursor_location);
    GetEventGenerator()->MoveMouseTo(cursor_location.x(), cursor_location.y());
    GetEventGenerator()->ClickLeftButton();
  }

  EXPECT_FALSE(is_expanded());

  {
    gfx::Point cursor_location(1, 1);
    views::View::ConvertPointToScreen(header_row(), &cursor_location);
    GetEventGenerator()->MoveMouseTo(cursor_location.x(), cursor_location.y());
    GetEventGenerator()->ClickLeftButton();
  }

  EXPECT_TRUE(is_expanded());
}

TEST_F(MediaNotificationViewTest, ActionButtonsHiddenByDefault) {
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPlay));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekForward));
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kSeekBackward));
}

TEST_F(MediaNotificationViewTest, ActionButtonsToggleVisbility) {
  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));

  EnableAction(MediaSessionAction::kNextTrack);

  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kNextTrack));

  DisableAction(MediaSessionAction::kNextTrack);

  EXPECT_FALSE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
}

TEST_F(MediaNotificationViewTest, UpdateArtworkFromItem) {
  gfx::Size size = view()->size();

  SkBitmap image;
  image.allocN32Pixels(10, 10);

  EXPECT_TRUE(GetArtworkImage().isNull());

  GetItem()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, image);

  // Ensure that the image is displayed in the background artwork and that the
  // size of the notification was not affected.
  EXPECT_FALSE(GetArtworkImage().isNull());
  EXPECT_EQ(gfx::Size(10, 10), GetArtworkImage().size());
  EXPECT_EQ(size, view()->size());

  GetItem()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, SkBitmap());

  // Ensure that the background artwork was reset and the size was still not
  // affected.
  EXPECT_TRUE(GetArtworkImage().isNull());
  EXPECT_EQ(size, view()->size());
}

}  // namespace ash
