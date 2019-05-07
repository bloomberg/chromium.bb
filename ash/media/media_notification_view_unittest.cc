// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_view.h"

#include <memory>

#include "ash/focus_cycler.h"
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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
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

// The title artist row should always have the same height.
const int kMediaTitleArtistRowExpectedHeight = 48;

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

    // Update the metadata.
    media_session::MediaMetadata metadata;
    metadata.title = base::ASCIIToUTF16("title");
    metadata.artist = base::ASCIIToUTF16("artist");
    GetItem()->MediaSessionMetadataChanged(metadata);

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
    const auto& children = button_row()->children();
    const auto i = std::find_if(
        children.begin(), children.end(), [action](const views::View* v) {
          return views::Button::AsButton(v)->tag() == static_cast<int>(action);
        });
    return (i == children.end()) ? nullptr : views::Button::AsButton(*i);
  }

  bool IsActionButtonVisible(MediaSessionAction action) const {
    return GetButtonForAction(action)->visible();
  }

  MediaNotificationItem* GetItem() const {
    return Shell::Get()->media_notification_controller()->GetItem(
        request_id_.ToString());
  }

  const base::UnguessableToken& request_id() const { return request_id_; }

  const gfx::ImageSkia& GetArtworkImage() const {
    return view_->GetMediaNotificationBackground()->artwork_;
  }

  const gfx::ImageSkia& GetAppIcon() const {
    return view_->header_row_->app_icon_for_testing();
  }

  bool expand_button_enabled() const {
    return header_row()->expand_button()->visible();
  }

  bool IsActuallyExpanded() const { return view_->IsActuallyExpanded(); }

  void SimulateButtonClick(MediaSessionAction action) {
    views::Button* button = GetButtonForAction(action);
    EXPECT_TRUE(button->visible());

    view_->ButtonPressed(
        button, ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               ui::EventTimeForNow(), 0, 0));
  }

  void SimulateTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EventFlags::EF_NONE);
  }

  void ExpectHistogramActionRecorded(MediaSessionAction action) {
    histogram_tester_.ExpectUniqueSample(
        MediaNotificationItem::kUserActionHistogramName,
        static_cast<base::HistogramBase::Sample>(action), 1);
  }

  void ExpectHistogramArtworkRecorded(bool present, int count) {
    histogram_tester_.ExpectBucketCount(
        MediaNotificationView::kArtworkHistogramName,
        static_cast<base::HistogramBase::Sample>(present), count);
  }

  void ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata metadata,
                                       int count) {
    histogram_tester_.ExpectBucketCount(
        MediaNotificationView::kMetadataHistogramName,
        static_cast<base::HistogramBase::Sample>(metadata), count);
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

  base::HistogramTester histogram_tester_;

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

  EXPECT_EQ(5u, button_row()->children().size());

  for (auto* child : button_row()->children()) {
    ASSERT_TRUE(IsMediaButtonType(child->GetClassName()));

    EXPECT_TRUE(child->visible());
    EXPECT_LT(kMediaButtonIconSize, child->width());
    EXPECT_LT(kMediaButtonIconSize, child->height());
    EXPECT_FALSE(views::Button::AsButton(child)->GetAccessibleName().empty());
  }

  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPlay));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kSeekBackward));
  EXPECT_TRUE(GetButtonForAction(MediaSessionAction::kSeekForward));

  // |kPause| cannot be present if |kPlay| is.
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause));
}

TEST_F(MediaNotificationViewTest, ButtonsFocusCheck) {
  EnableAllActions();

  Shell::Get()->focus_cycler()->FocusWidget(view()->GetWidget());
  views::FocusManager* focus_manager = view()->GetFocusManager();

  {
    // Focus the first action button.
    auto* button = GetButtonForAction(MediaSessionAction::kPreviousTrack);
    focus_manager->SetFocusedView(button);
    EXPECT_EQ(button, focus_manager->GetFocusedView());
  }

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kSeekBackward),
            focus_manager->GetFocusedView());

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kPlay),
            focus_manager->GetFocusedView());

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kSeekForward),
            focus_manager->GetFocusedView());

  SimulateTab();
  EXPECT_EQ(GetButtonForAction(MediaSessionAction::kNextTrack),
            focus_manager->GetFocusedView());
}

TEST_F(MediaNotificationViewTest, PlayPauseButtonTooltipCheck) {
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  auto* button = GetButtonForAction(MediaSessionAction::kPlay);
  base::string16 tooltip = button->GetTooltipText(gfx::Point());
  EXPECT_FALSE(tooltip.empty());

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  session_info->is_controllable = true;
  GetItem()->MediaSessionInfoChanged(session_info.Clone());

  base::string16 new_tooltip = button->GetTooltipText(gfx::Point());
  EXPECT_FALSE(new_tooltip.empty());
  EXPECT_NE(tooltip, new_tooltip);
}

TEST_F(MediaNotificationViewTest, NextTrackButtonClick) {
  EnableAction(MediaSessionAction::kNextTrack);

  EXPECT_EQ(0, media_controller()->next_track_count());

  SimulateButtonClick(MediaSessionAction::kNextTrack);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->next_track_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kNextTrack);
}

TEST_F(MediaNotificationViewTest, PlayButtonClick) {
  EnableAction(MediaSessionAction::kPlay);

  EXPECT_EQ(0, media_controller()->resume_count());

  SimulateButtonClick(MediaSessionAction::kPlay);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->resume_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kPlay);
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

  SimulateButtonClick(MediaSessionAction::kPause);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->suspend_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kPause);
}

TEST_F(MediaNotificationViewTest, PreviousTrackButtonClick) {
  EnableAction(MediaSessionAction::kPreviousTrack);

  EXPECT_EQ(0, media_controller()->previous_track_count());

  SimulateButtonClick(MediaSessionAction::kPreviousTrack);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->previous_track_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kPreviousTrack);
}

TEST_F(MediaNotificationViewTest, SeekBackwardButtonClick) {
  EnableAction(MediaSessionAction::kSeekBackward);

  EXPECT_EQ(0, media_controller()->seek_backward_count());

  SimulateButtonClick(MediaSessionAction::kSeekBackward);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->seek_backward_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kSeekBackward);
}

TEST_F(MediaNotificationViewTest, SeekForwardButtonClick) {
  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_EQ(0, media_controller()->seek_forward_count());

  SimulateButtonClick(MediaSessionAction::kSeekForward);
  GetItem()->FlushForTesting();

  EXPECT_EQ(1, media_controller()->seek_forward_count());
  ExpectHistogramActionRecorded(MediaSessionAction::kSeekForward);
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

TEST_F(MediaNotificationViewTest, MetadataIsDisplayed) {
  EnableAllActions();

  EXPECT_TRUE(title_artist_row()->visible());
  EXPECT_TRUE(title_label()->visible());
  EXPECT_TRUE(artist_label()->visible());

  EXPECT_EQ(base::ASCIIToUTF16("title"), title_label()->text());
  EXPECT_EQ(base::ASCIIToUTF16("artist"), artist_label()->text());

  EXPECT_EQ(kMediaTitleArtistRowExpectedHeight, title_artist_row()->height());
}

TEST_F(MediaNotificationViewTest, UpdateMetadata_FromObserver) {
  EnableAllActions();

  ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata::kTitle, 1);
  ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata::kArtist, 1);
  ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata::kAlbum, 0);
  ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata::kCount, 1);

  EXPECT_FALSE(header_row()->summary_text_for_testing()->visible());

  media_session::MediaMetadata metadata;
  metadata.title = base::ASCIIToUTF16("title2");
  metadata.artist = base::ASCIIToUTF16("artist2");
  metadata.album = base::ASCIIToUTF16("album");

  GetItem()->MediaSessionMetadataChanged(metadata);

  EXPECT_TRUE(title_artist_row()->visible());
  EXPECT_TRUE(title_label()->visible());
  EXPECT_TRUE(artist_label()->visible());
  EXPECT_TRUE(header_row()->summary_text_for_testing()->visible());

  EXPECT_EQ(metadata.title, title_label()->text());
  EXPECT_EQ(metadata.artist, artist_label()->text());
  EXPECT_EQ(metadata.album, header_row()->summary_text_for_testing()->text());

  EXPECT_EQ(kMediaTitleArtistRowExpectedHeight, title_artist_row()->height());

  ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata::kTitle, 2);
  ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata::kArtist, 2);
  ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata::kAlbum, 1);
  ExpectHistogramMetadataRecorded(MediaNotificationView::Metadata::kCount, 2);
}

TEST_F(MediaNotificationViewTest, UpdateMetadata_AppName) {
  EXPECT_EQ(
      message_center::MessageCenter::Get()->GetSystemNotificationAppName(),
      header_row()->app_name_for_testing());

  {
    media_session::MediaMetadata metadata;
    metadata.title = base::ASCIIToUTF16("title");
    metadata.artist = base::ASCIIToUTF16("artist");
    metadata.source_title = base::ASCIIToUTF16(kTestAppName);
    GetItem()->MediaSessionMetadataChanged(metadata);
  }

  EXPECT_EQ(base::ASCIIToUTF16(kTestAppName),
            header_row()->app_name_for_testing());

  {
    media_session::MediaMetadata metadata;
    metadata.title = base::ASCIIToUTF16("title");
    metadata.artist = base::ASCIIToUTF16("artist");
    GetItem()->MediaSessionMetadataChanged(metadata);
  }

  EXPECT_EQ(
      message_center::MessageCenter::Get()->GetSystemNotificationAppName(),
      header_row()->app_name_for_testing());
}

TEST_F(MediaNotificationViewTest, Buttons_WhenCollapsed) {
  EnableAllActions();

  view()->SetExpanded(false);

  EXPECT_FALSE(IsActuallyExpanded());

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

  view()->SetExpanded(true);

  EXPECT_TRUE(IsActuallyExpanded());

  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPlay));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kPreviousTrack));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kNextTrack));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kSeekBackward));
  EXPECT_TRUE(IsActionButtonVisible(MediaSessionAction::kSeekForward));
}

TEST_F(MediaNotificationViewTest, ClickHeader_ToggleExpand) {
  EnableAllActions();

  EXPECT_TRUE(IsActuallyExpanded());

  {
    gfx::Point cursor_location(1, 1);
    views::View::ConvertPointToScreen(header_row(), &cursor_location);
    GetEventGenerator()->MoveMouseTo(cursor_location.x(), cursor_location.y());
    GetEventGenerator()->ClickLeftButton();
  }

  EXPECT_FALSE(IsActuallyExpanded());

  {
    gfx::Point cursor_location(1, 1);
    views::View::ConvertPointToScreen(header_row(), &cursor_location);
    GetEventGenerator()->MoveMouseTo(cursor_location.x(), cursor_location.y());
    GetEventGenerator()->ClickLeftButton();
  }

  EXPECT_TRUE(IsActuallyExpanded());
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
  int title_artist_width = title_artist_row()->width();
  const SkColor accent = header_row()->accent_color_for_testing();
  gfx::Size size = view()->size();

  SkBitmap image;
  image.allocN32Pixels(10, 10);
  image.eraseColor(SK_ColorMAGENTA);

  EXPECT_TRUE(GetArtworkImage().isNull());

  GetItem()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, image);

  ExpectHistogramArtworkRecorded(true, 1);

  // Ensure the title artist row has a small width than before now that we
  // have artwork.
  EXPECT_GT(title_artist_width, title_artist_row()->width());

  // Ensure that the image is displayed in the background artwork and that the
  // size of the notification was not affected.
  EXPECT_FALSE(GetArtworkImage().isNull());
  EXPECT_EQ(gfx::Size(10, 10), GetArtworkImage().size());
  EXPECT_EQ(size, view()->size());
  EXPECT_NE(accent, header_row()->accent_color_for_testing());

  GetItem()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kArtwork, SkBitmap());

  ExpectHistogramArtworkRecorded(false, 1);

  // Ensure the title artist row goes back to the original width now that we
  // do not have any artwork.
  EXPECT_EQ(title_artist_width, title_artist_row()->width());

  // Ensure that the background artwork was reset and the size was still not
  // affected.
  EXPECT_TRUE(GetArtworkImage().isNull());
  EXPECT_EQ(size, view()->size());
  EXPECT_EQ(accent, header_row()->accent_color_for_testing());
}

TEST_F(MediaNotificationViewTest, UpdateIconFromItem) {
  gfx::ImageSkia original = GetAppIcon();
  EXPECT_EQ(message_center::kSmallImageSizeMD, original.width());
  EXPECT_EQ(message_center::kSmallImageSizeMD, original.height());

  // The size for the image we provide should be different so we can compare.
  const int alt_size = message_center::kSmallImageSizeMD + 1;

  SkBitmap bitmap;
  bitmap.allocN32Pixels(alt_size, alt_size);

  GetItem()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kSourceIcon, bitmap);

  EXPECT_EQ(alt_size, GetAppIcon().width());
  EXPECT_EQ(alt_size, GetAppIcon().height());

  GetItem()->MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kSourceIcon, SkBitmap());

  EXPECT_EQ(message_center::kSmallImageSizeMD, GetAppIcon().width());
  EXPECT_EQ(message_center::kSmallImageSizeMD, GetAppIcon().height());
}

TEST_F(MediaNotificationViewTest, ExpandableDefaultState) {
  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());
}

TEST_F(MediaNotificationViewTest, ExpandablePlayPauseActionCountsOnce) {
  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAction(MediaSessionAction::kPreviousTrack);
  EnableAction(MediaSessionAction::kNextTrack);
  EnableAction(MediaSessionAction::kPlay);
  EnableAction(MediaSessionAction::kPause);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  media_session::mojom::MediaSessionInfoPtr session_info(
      media_session::mojom::MediaSessionInfo::New());
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPlaying;
  view()->UpdateWithMediaSessionInfo(session_info);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());
}

TEST_F(MediaNotificationViewTest, BecomeExpandableAndWasNotExpandable) {
  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAllActions();

  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());
}

TEST_F(MediaNotificationViewTest, BecomeExpandableButWasAlreadyExpandable) {
  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAllActions();

  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());

  DisableAction(MediaSessionAction::kSeekForward);

  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());
}

TEST_F(MediaNotificationViewTest, BecomeNotExpandableAndWasExpandable) {
  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAllActions();

  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_TRUE(expand_button_enabled());

  DisableAction(MediaSessionAction::kPreviousTrack);
  DisableAction(MediaSessionAction::kNextTrack);
  DisableAction(MediaSessionAction::kSeekBackward);
  DisableAction(MediaSessionAction::kSeekForward);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());
}

TEST_F(MediaNotificationViewTest,
       BecomeNotExpandableButWasAlreadyNotExpandable) {
  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());

  EnableAction(MediaSessionAction::kSeekForward);

  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_FALSE(expand_button_enabled());
}

TEST_F(MediaNotificationViewTest, ActionButtonRowSizeAndAlignment) {
  EnableAction(MediaSessionAction::kPlay);

  views::Button* button = GetButtonForAction(MediaSessionAction::kPlay);
  int button_x = button->GetBoundsInScreen().x();

  // When collapsed the button row should be a fixed width.
  EXPECT_FALSE(IsActuallyExpanded());
  EXPECT_EQ(124, button_row()->width());

  EnableAllActions();
  view()->SetExpanded(true);

  // When expanded the button row should be wider and the play button should
  // have shifted to the left.
  EXPECT_TRUE(IsActuallyExpanded());
  EXPECT_LT(124, button_row()->width());
  EXPECT_GT(button_x, button->GetBoundsInScreen().x());
}

}  // namespace ash
