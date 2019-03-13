// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_view.h"

#include "ash/media/media_notification_background.h"
#include "ash/media/media_notification_constants.h"
#include "ash/media/media_notification_controller.h"
#include "ash/shell.h"
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

namespace ash {

using media_session::mojom::MediaSessionAction;

namespace {

// The right padding is 1/5th the size of the notification.
constexpr int kRightMarginSize = message_center::kNotificationWidth / 5;

// The right padding is 1/3rd the size of the notification when the
// notification is expanded.
constexpr int kRightMarginExpandedSize = message_center::kNotificationWidth / 3;

// Dimensions.
constexpr int kDefaultMarginSize = 16;
constexpr int kMediaButtonIconSize = 24;
constexpr double kMediaImageMaxWidthPct = 0.3;
constexpr double kMediaImageMaxWidthExpandedPct = 0.4;

SkColor GetMediaNotificationColor(const views::View& view) {
  return views::style::GetColor(view, views::style::CONTEXT_LABEL,
                                views::style::STYLE_PRIMARY);
}

bool ShouldShowActionWhenCollapsed(MediaSessionAction action) {
  return action == MediaSessionAction::kPlay ||
         action == MediaSessionAction::kPause ||
         action == MediaSessionAction::kNextTrack ||
         action == MediaSessionAction::kPreviousTrack;
}

}  // namespace

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
  header_row_->SetExpandButtonEnabled(true);
  header_row_->SetAppName(
      message_center::MessageCenter::Get()->GetSystemNotificationAppName());
  AddChildView(header_row_);

  // |main_row_| holds the main content of the notification.
  main_row_ = new views::View();
  AddChildView(main_row_);

  // |title_artist_row_| contains the title and artist labels.
  title_artist_row_ = new views::View();
  auto* title_artist_row_layout =
      title_artist_row_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kVertical, gfx::Insets(), 0));
  title_artist_row_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  title_artist_row_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  title_artist_row_->SetVisible(false);
  main_row_->AddChildView(title_artist_row_);

  title_label_ = new views::Label(base::string16(), views::style::CONTEXT_LABEL,
                                  views::style::STYLE_PRIMARY);
  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
  title_label_->SetFontList(base_font_list.Derive(
      0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::SEMIBOLD));
  title_label_->SetVisible(false);
  title_artist_row_->AddChildView(title_label_);

  artist_label_ =
      new views::Label(base::string16(), views::style::CONTEXT_LABEL,
                       views::style::STYLE_PRIMARY);
  artist_label_->SetVisible(false);
  title_artist_row_->AddChildView(artist_label_);

  // |button_row_| contains the buttons for controlling playback.
  button_row_ = new views::View();
  auto* button_row_layout =
      button_row_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal, gfx::Insets(), 0));
  button_row_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  button_row_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  button_row_layout->SetDefaultFlex(1);
  main_row_->AddChildView(button_row_);

  CreateMediaButton(vector_icons::kMediaPreviousTrackIcon,
                    MediaSessionAction::kPreviousTrack);
  CreateMediaButton(vector_icons::kMediaSeekBackwardIcon,
                    MediaSessionAction::kSeekBackward);

  // |play_pause_button_| toggles playback.
  play_pause_button_ = views::CreateVectorToggleImageButton(this);
  play_pause_button_->set_tag(static_cast<int>(MediaSessionAction::kPlay));
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
  play_pause_button_->SetVisible(IsActionButtonVisible(action));

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationView::UpdateWithMediaMetadata(
    const media_session::MediaMetadata& metadata) {
  if (!metadata.source_title.empty()) {
    header_row_->SetAppName(metadata.source_title);
  } else {
    header_row_->SetAppName(
        message_center::MessageCenter::Get()->GetSystemNotificationAppName());
  }

  if (metadata.title.empty() && metadata.artist.empty()) {
    title_artist_row_->SetVisible(false);
  } else {
    title_artist_row_->SetVisible(true);

    title_label_->SetText(metadata.title);
    title_label_->SetVisible(!metadata.title.empty());

    artist_label_->SetText(metadata.artist);
    artist_label_->SetVisible(!metadata.artist.empty());
  }

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationView::UpdateWithMediaActions(
    const std::set<media_session::mojom::MediaSessionAction>& actions) {
  enabled_actions_ = actions;
  UpdateActionButtonsVisibility();

  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void MediaNotificationView::UpdateWithMediaArtwork(
    const gfx::ImageSkia& image) {
  GetMediaNotificationBackground()->UpdateArtwork(image);
}

void MediaNotificationView::UpdateControlButtonsVisibilityWithNotification(
    const message_center::Notification& notification) {
  // Media notifications do not use the settings and snooze buttons.
  DCHECK(!notification.should_show_settings_button());
  DCHECK(!notification.should_show_snooze_button());

  control_buttons_view_->ShowCloseButton(!notification.pinned());
  UpdateControlButtonsVisibility();
}

bool MediaNotificationView::IsActionButtonVisible(
    MediaSessionAction action) const {
  // Not all media sessions support the same actions.
  bool visible = base::ContainsKey(enabled_actions_, action);

  // We should reduce the number of action buttons we show when we are
  // collapsed.
  // TODO(beccahughes): Use priority based ranking here
  if (!expanded_ && visible)
    visible = ShouldShowActionWhenCollapsed(action);

  return visible;
}

void MediaNotificationView::UpdateActionButtonsVisibility() {
  for (int i = 0; i < button_row_->child_count(); ++i) {
    views::Button* action_button =
        views::Button::AsButton(button_row_->child_at(i));

    action_button->SetVisible(IsActionButtonVisible(
        static_cast<MediaSessionAction>(action_button->tag())));
  }
}

void MediaNotificationView::UpdateViewForExpandedState() {
  // Adjust the layout of the |main_row_| based on the expanded state. If the
  // notification is expanded then the buttons should be below the title/artist
  // information. If it is collapsed then the buttons will be to the right.
  if (expanded_) {
    main_row_
        ->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::kVertical,
            gfx::Insets(kDefaultMarginSize, kDefaultMarginSize,
                        kDefaultMarginSize, kRightMarginExpandedSize),
            kDefaultMarginSize))
        ->SetDefaultFlex(1);
  } else {
    main_row_
        ->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::kHorizontal,
            gfx::Insets(0, kDefaultMarginSize, kDefaultMarginSize,
                        kRightMarginSize),
            kDefaultMarginSize, true))
        ->SetDefaultFlex(1);
  }

  GetMediaNotificationBackground()->UpdateArtworkMaxWidthPct(
      expanded_ ? kMediaImageMaxWidthExpandedPct : kMediaImageMaxWidthPct);

  header_row_->SetExpanded(expanded_);

  UpdateActionButtonsVisibility();
}

void MediaNotificationView::CreateMediaButton(const gfx::VectorIcon& icon,
                                              MediaSessionAction action) {
  views::ImageButton* button = views::CreateVectorImageButton(this);
  button->set_tag(static_cast<int>(action));
  views::SetImageFromVectorIcon(button, icon, kMediaButtonIconSize,
                                GetMediaNotificationColor(*button));
  button_row_->AddChildView(button);
}

MediaNotificationBackground*
MediaNotificationView::GetMediaNotificationBackground() {
  return static_cast<MediaNotificationBackground*>(background());
}

}  // namespace ash
