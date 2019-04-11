// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_view.h"

#include "ash/media/media_notification_background.h"
#include "ash/media/media_notification_constants.h"
#include "ash/media/media_notification_controller.h"
#include "ash/shell.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/vector_icons/vector_icons.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace ash {

using media_session::mojom::MediaSessionAction;

namespace {

// The right padding is 1/5th the size of the notification.
constexpr int kRightMarginSize = message_center::kNotificationWidth / 5;

// The right padding is 1/3rd the size of the notification when the
// notification is expanded.
constexpr int kRightMarginExpandedSize = message_center::kNotificationWidth / 4;

// The number of actions supported when the notification is expanded or not.
constexpr size_t kMediaNotificationActionsCount = 3;
constexpr size_t kMediaNotificationExpandedActionsCount = 5;

// Dimensions.
constexpr int kDefaultMarginSize = 8;
constexpr int kMediaButtonIconSize = 28;
constexpr int kTitleArtistLineHeight = 20;
constexpr double kMediaImageMaxWidthPct = 0.3;
constexpr double kMediaImageMaxWidthExpandedPct = 0.4;
constexpr gfx::Size kMediaButtonSize = gfx::Size(36, 36);
constexpr int kMediaButtonRowSeparator = 8;
constexpr gfx::Insets kMediaTitleArtistInsets = gfx::Insets(8, 8, 0, 8);
constexpr int kMediaNotificationHeaderTopInset = 6;
constexpr int kMediaNotificationHeaderRightInset = 6;
constexpr int kMediaNotificationHeaderInset = 0;

// The action buttons in order of preference. If there is not enough space to
// show all the action buttons then this is used to determine which will be
// shown.
constexpr MediaSessionAction kMediaNotificationPreferredActions[] = {
    MediaSessionAction::kPlay,          MediaSessionAction::kPause,
    MediaSessionAction::kPreviousTrack, MediaSessionAction::kNextTrack,
    MediaSessionAction::kSeekBackward,  MediaSessionAction::kSeekForward,
};

SkColor GetMediaNotificationColor(const views::View& view) {
  return views::style::GetColor(view, views::style::CONTEXT_LABEL,
                                views::style::STYLE_PRIMARY);
}

void RecordMetadataHistogram(MediaNotificationView::Metadata metadata) {
  UMA_HISTOGRAM_ENUMERATION(MediaNotificationView::kMetadataHistogramName,
                            metadata);
}

}  // namespace

// static
const char MediaNotificationView::kArtworkHistogramName[] =
    "Media.Notification.ArtworkPresent";

// static
const char MediaNotificationView::kMetadataHistogramName[] =
    "Media.Notification.MetadataPresent";

MediaNotificationView::MediaNotificationView(
    const message_center::Notification& notification)
    : message_center::MessageView(notification) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(), 0));

  // |controls_button_view_| has the common notification control buttons.
  control_buttons_view_ =
      std::make_unique<message_center::NotificationControlButtonsView>(this);
  control_buttons_view_->set_owned_by_client();

  // |header_row_| contains app_icon, app_name, control buttons, etc.
  header_row_ = new message_center::NotificationHeaderView(
      control_buttons_view_.get(), this);
  header_row_->SetAppName(
      message_center::MessageCenter::Get()->GetSystemNotificationAppName());
  header_row_->ClearAppIcon();
  header_row_->SetProperty(views::kMarginsKey,
                           new gfx::Insets(kMediaNotificationHeaderTopInset,
                                           kMediaNotificationHeaderInset,
                                           kMediaNotificationHeaderInset,
                                           kMediaNotificationHeaderRightInset));
  AddChildView(header_row_);

  // |main_row_| holds the main content of the notification.
  main_row_ = new views::View();
  AddChildView(main_row_);

  // |title_artist_row_| contains the title and artist labels.
  title_artist_row_ = new views::View();
  title_artist_row_layout_ =
      title_artist_row_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kVertical, kMediaTitleArtistInsets, 0));
  title_artist_row_layout_->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  title_artist_row_layout_->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  main_row_->AddChildView(title_artist_row_);

  title_label_ = new views::Label(base::string16(), views::style::CONTEXT_LABEL,
                                  views::style::STYLE_PRIMARY);
  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
  title_label_->SetFontList(base_font_list.Derive(
      0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::MEDIUM));
  title_label_->SetLineHeight(kTitleArtistLineHeight);
  title_artist_row_->AddChildView(title_label_);

  artist_label_ =
      new views::Label(base::string16(), views::style::CONTEXT_LABEL,
                       views::style::STYLE_PRIMARY);
  artist_label_->SetLineHeight(kTitleArtistLineHeight);
  title_artist_row_->AddChildView(artist_label_);

  // |button_row_| contains the buttons for controlling playback.
  button_row_ = new views::View();
  auto* button_row_layout =
      button_row_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal, gfx::Insets(),
          kMediaButtonRowSeparator));
  button_row_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  button_row_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  main_row_->AddChildView(button_row_);

  CreateMediaButton(vector_icons::kMediaPreviousTrackIcon,
                    MediaSessionAction::kPreviousTrack);
  CreateMediaButton(vector_icons::kMediaSeekBackwardIcon,
                    MediaSessionAction::kSeekBackward);

  // |play_pause_button_| toggles playback.
  play_pause_button_ = views::CreateVectorToggleImageButton(this);
  play_pause_button_->set_tag(static_cast<int>(MediaSessionAction::kPlay));
  play_pause_button_->SetPreferredSize(kMediaButtonSize);
  SkColor play_button_color = GetMediaNotificationColor(*play_pause_button_);
  views::SetImageFromVectorIcon(play_pause_button_,
                                vector_icons::kPlayArrowIcon,
                                kMediaButtonIconSize, play_button_color);
  views::SetToggledImageFromVectorIcon(play_pause_button_,
                                       vector_icons::kPauseIcon,
                                       kMediaButtonIconSize, play_button_color);
  button_row_->AddChildView(play_pause_button_);

  CreateMediaButton(vector_icons::kMediaSeekForwardIcon,
                    MediaSessionAction::kSeekForward);
  CreateMediaButton(vector_icons::kMediaNextTrackIcon,
                    MediaSessionAction::kNextTrack);

  SetBackground(std::make_unique<MediaNotificationBackground>(
      this, message_center::kNotificationCornerRadius,
      message_center::kNotificationCornerRadius, kMediaImageMaxWidthPct));

  UpdateControlButtonsVisibilityWithNotification(notification);
  UpdateCornerRadius(message_center::kNotificationCornerRadius,
                     message_center::kNotificationCornerRadius);
  UpdateViewForExpandedState();

  Shell::Get()->media_notification_controller()->SetView(notification_id(),
                                                         this);
}

MediaNotificationView::~MediaNotificationView() {
  Shell::Get()->media_notification_controller()->SetView(notification_id(),
                                                         nullptr);
}

void MediaNotificationView::UpdateWithNotification(
    const message_center::Notification& notification) {
  MessageView::UpdateWithNotification(notification);

  UpdateControlButtonsVisibilityWithNotification(notification);

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

message_center::NotificationControlButtonsView*
MediaNotificationView::GetControlButtonsView() const {
  return control_buttons_view_.get();
}

void MediaNotificationView::SetExpanded(bool expanded) {
  if (expanded_ == expanded)
    return;

  expanded_ = expanded;

  UpdateViewForExpandedState();

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationView::UpdateCornerRadius(int top_radius,
                                               int bottom_radius) {
  GetMediaNotificationBackground()->UpdateCornerRadius(top_radius,
                                                       bottom_radius);
}

void MediaNotificationView::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
      UpdateControlButtonsVisibility();
      break;
    default:
      break;
  }

  View::OnMouseEvent(event);
}

void MediaNotificationView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (sender == header_row_) {
    SetExpanded(!expanded_);
    return;
  }

  if (sender->parent() == button_row_) {
    message_center::MessageCenter::Get()->ClickOnNotificationButton(
        notification_id(), sender->tag());
    return;
  }

  NOTREACHED();
}

void MediaNotificationView::UpdateWithMediaSessionInfo(
    const media_session::mojom::MediaSessionInfoPtr& session_info) {
  bool playing =
      session_info && session_info->playback_state ==
                          media_session::mojom::MediaPlaybackState::kPlaying;
  play_pause_button_->SetToggled(playing);

  MediaSessionAction action =
      playing ? MediaSessionAction::kPause : MediaSessionAction::kPlay;
  play_pause_button_->set_tag(static_cast<int>(action));

  UpdateActionButtonsVisibility();

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationView::UpdateWithMediaMetadata(
    const media_session::MediaMetadata& metadata) {
  header_row_->SetAppName(
      metadata.source_title.empty()
          ? message_center::MessageCenter::Get()->GetSystemNotificationAppName()
          : metadata.source_title);

  title_label_->SetText(metadata.title);
  artist_label_->SetText(metadata.artist);
  header_row_->SetSummaryText(metadata.album);

  if (!metadata.title.empty())
    RecordMetadataHistogram(Metadata::kTitle);

  if (!metadata.artist.empty())
    RecordMetadataHistogram(Metadata::kArtist);

  if (!metadata.album.empty())
    RecordMetadataHistogram(Metadata::kAlbum);

  RecordMetadataHistogram(Metadata::kCount);

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationView::UpdateWithMediaActions(
    const std::set<media_session::mojom::MediaSessionAction>& actions) {
  enabled_actions_ = actions;

  header_row_->SetExpandButtonEnabled(IsExpandable());
  UpdateViewForExpandedState();

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationView::UpdateWithMediaArtwork(
    const gfx::ImageSkia& image) {
  GetMediaNotificationBackground()->UpdateArtwork(image);

  has_artwork_ = !image.isNull();
  UpdateViewForExpandedState();

  UMA_HISTOGRAM_BOOLEAN(kArtworkHistogramName, has_artwork_);

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationView::UpdateWithMediaIcon(const gfx::ImageSkia& image) {
  if (image.isNull()) {
    header_row_->ClearAppIcon();
  } else {
    header_row_->SetAppIcon(image);
  }
}

void MediaNotificationView::UpdateControlButtonsVisibilityWithNotification(
    const message_center::Notification& notification) {
  // Media notifications do not use the settings and snooze buttons.
  DCHECK(!notification.should_show_settings_button());
  DCHECK(!notification.should_show_snooze_button());

  control_buttons_view_->ShowCloseButton(!notification.pinned());
  UpdateControlButtonsVisibility();
}

void MediaNotificationView::UpdateActionButtonsVisibility() {
  std::set<MediaSessionAction> visible_actions =
      CalculateVisibleActions(IsActuallyExpanded());

  for (int i = 0; i < button_row_->child_count(); ++i) {
    views::Button* action_button =
        views::Button::AsButton(button_row_->child_at(i));

    action_button->SetVisible(base::ContainsKey(
        visible_actions,
        static_cast<MediaSessionAction>(action_button->tag())));
  }
}

void MediaNotificationView::UpdateViewForExpandedState() {
  bool expanded = IsActuallyExpanded();

  // Adjust the layout of the |main_row_| based on the expanded state. If the
  // notification is expanded then the buttons should be below the title/artist
  // information. If it is collapsed then the buttons will be to the right.
  if (expanded) {
    main_row_
        ->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::kVertical,
            gfx::Insets(
                kDefaultMarginSize, kDefaultMarginSize, kDefaultMarginSize,
                has_artwork_ ? kRightMarginExpandedSize : kDefaultMarginSize),
            kDefaultMarginSize))
        ->SetDefaultFlex(1);
  } else {
    main_row_
        ->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::kHorizontal,
            gfx::Insets(0, kDefaultMarginSize, 14,
                        has_artwork_ ? kRightMarginSize : kDefaultMarginSize),
            kDefaultMarginSize, true))
        ->SetDefaultFlex(1);
  }

  main_row_->Layout();

  GetMediaNotificationBackground()->UpdateArtworkMaxWidthPct(
      expanded ? kMediaImageMaxWidthExpandedPct : kMediaImageMaxWidthPct);

  header_row_->SetExpanded(expanded);

  UpdateActionButtonsVisibility();
}

void MediaNotificationView::CreateMediaButton(const gfx::VectorIcon& icon,
                                              MediaSessionAction action) {
  views::ImageButton* button = views::CreateVectorImageButton(this);
  button->set_tag(static_cast<int>(action));
  views::SetImageFromVectorIcon(button, icon, kMediaButtonIconSize,
                                GetMediaNotificationColor(*button));
  button->SetPreferredSize(kMediaButtonSize);
  button_row_->AddChildView(button);
}

MediaNotificationBackground*
MediaNotificationView::GetMediaNotificationBackground() {
  return static_cast<MediaNotificationBackground*>(background());
}

bool MediaNotificationView::IsExpandable() const {
  // If we can show more notifications if we were expanded then we should be
  // expandable.
  return CalculateVisibleActions(true).size() > kMediaNotificationActionsCount;
}

bool MediaNotificationView::IsActuallyExpanded() const {
  return expanded_ && IsExpandable();
}

std::set<MediaSessionAction> MediaNotificationView::CalculateVisibleActions(
    bool expanded) const {
  size_t max_actions = expanded ? kMediaNotificationExpandedActionsCount
                                : kMediaNotificationActionsCount;
  std::set<MediaSessionAction> visible_actions;

  for (auto& action : kMediaNotificationPreferredActions) {
    if (visible_actions.size() >= max_actions)
      break;

    // If the action is play or pause then we should only make it visible if the
    // current play pause button has that action.
    if ((action == MediaSessionAction::kPlay ||
         action == MediaSessionAction::kPause) &&
        static_cast<MediaSessionAction>(play_pause_button_->tag()) != action) {
      continue;
    }

    if (!base::ContainsKey(enabled_actions_, action))
      continue;

    visible_actions.insert(action);
  }

  return visible_actions;
}

}  // namespace ash
