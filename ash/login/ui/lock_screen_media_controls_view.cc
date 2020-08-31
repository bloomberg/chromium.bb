// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_screen_media_controls_view.h"

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/media_controls_header_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/media/media_controller_impl.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "base/metrics/histogram_functions.h"
#include "base/power_monitor/power_monitor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/media_message_center/media_controls_progress_view.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/vector_icons/vector_icons.h"
#include "services/media_session/public/cpp/util.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/media_session/public/mojom/media_session_service.mojom.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/vector_icons.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace ash {

using media_session::mojom::MediaSessionAction;

namespace {

constexpr SkColor kProgressBarForeground = gfx::kGoogleBlue300;
constexpr SkColor kProgressBarBackground =
    SkColorSetA(gfx::kGoogleBlue300, 0x4C);  // 30%

// Maximum number of actions that should be displayed on |button_row_|.
constexpr size_t kMaxActions = 5;

// Dimensions.
constexpr gfx::Insets kMediaControlsInsets = gfx::Insets(16, 16, 16, 16);
constexpr int kMediaControlsCornerRadius = 16;
constexpr int kMinimumSourceIconSize = 16;
constexpr int kDesiredSourceIconSize = 20;
constexpr int kMinimumArtworkSize = 30;
constexpr int kDesiredArtworkSize = 48;
constexpr int kArtworkRowPadding = 16;
constexpr gfx::Insets kArtworkRowInsets = gfx::Insets(24, 0, 9, 0);
constexpr gfx::Size kArtworkRowPreferredSize =
    gfx::Size(328, kDesiredArtworkSize);
constexpr int kMediaButtonRowPadding = 16;
constexpr gfx::Insets kButtonRowInsets = gfx::Insets(4, 0, 0, 0);
constexpr int kPlayPauseIconSize = 40;
constexpr int kMediaControlsIconSize = 24;
constexpr gfx::Size kPlayPauseButtonSize = gfx::Size(72, 72);
constexpr gfx::Size kMediaControlsButtonSize = gfx::Size(48, 48);
constexpr gfx::Size kMediaControlsButtonRowSize =
    gfx::Size(328, kPlayPauseButtonSize.height());
constexpr gfx::Size kMediaButtonGroupSize =
    gfx::Size(2 * kMediaControlsButtonSize.width() + kMediaButtonRowPadding,
              kPlayPauseButtonSize.height());
constexpr int kArtworkCornerRadius = 4;

constexpr int kDragVelocityThreshold = 6;
constexpr int kDistanceDismissalThreshold = 20;
constexpr base::TimeDelta kAnimationDuration =
    base::TimeDelta::FromMilliseconds(200);

// How long to wait (in milliseconds) for a new media session to begin.
constexpr base::TimeDelta kNextMediaDelay =
    base::TimeDelta::FromMilliseconds(2500);

constexpr const char kLockScreenMediaControlsViewName[] =
    "LockScreenMediaControlsView";

// Scales |size| to fit |view_size| while preserving proportions.
gfx::Size ScaleSizeToFitView(const gfx::Size& size,
                             const gfx::Size& view_size) {
  // If |size| is too big in either dimension or two small in both
  // dimensions, scale it appropriately.
  if ((size.width() > view_size.width() ||
       size.height() > view_size.height()) ||
      (size.width() < view_size.width() &&
       size.height() < view_size.height())) {
    const float scale =
        std::min(view_size.width() / static_cast<float>(size.width()),
                 view_size.height() / static_cast<float>(size.height()));
    return gfx::ScaleToFlooredSize(size, scale);
  }

  return size;
}

const gfx::VectorIcon& GetVectorIconForMediaAction(MediaSessionAction action) {
  switch (action) {
    case MediaSessionAction::kPreviousTrack:
      return vector_icons::kMediaPreviousTrackIcon;
    case MediaSessionAction::kPause:
      return vector_icons::kPauseIcon;
    case MediaSessionAction::kNextTrack:
      return vector_icons::kMediaNextTrackIcon;
    case MediaSessionAction::kPlay:
      return vector_icons::kPlayArrowIcon;
    case MediaSessionAction::kSeekBackward:
      return vector_icons::kMediaSeekBackwardIcon;
    case MediaSessionAction::kSeekForward:
      return vector_icons::kMediaSeekForwardIcon;

    // The following actions are not yet supported on the controls.
    case MediaSessionAction::kStop:
    case MediaSessionAction::kSkipAd:
    case MediaSessionAction::kSeekTo:
    case MediaSessionAction::kScrubTo:
    case MediaSessionAction::kEnterPictureInPicture:
    case MediaSessionAction::kExitPictureInPicture:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return gfx::kNoneIcon;
}

// MediaActionButton is an image button with a custom ink drop mask.
class MediaActionButton : public views::ImageButton {
 public:
  MediaActionButton(views::ButtonListener* listener,
                    int icon_size,
                    MediaSessionAction action,
                    const base::string16& accessible_name)
      : views::ImageButton(listener),
        icon_size_(icon_size) {
    SetInkDropMode(views::Button::InkDropMode::ON);
    set_has_ink_drop_action_on_click(true);
    SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    SetBorder(
        views::CreateEmptyBorder(views::LayoutProvider::Get()->GetInsetsMetric(
            views::INSETS_VECTOR_IMAGE_BUTTON)));

    const bool is_play_pause = action == MediaSessionAction::kPause ||
                               action == MediaSessionAction::kPlay;
    SetPreferredSize(is_play_pause ? kPlayPauseButtonSize
                                   : kMediaControlsButtonSize);
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    SetAction(action, accessible_name);

    SetInstallFocusRingOnFocus(true);
    login_views_utils::ConfigureRectFocusRingCircleInkDrop(this, focus_ring(),
                                                           base::nullopt);
  }

  ~MediaActionButton() override = default;

  void SetAction(MediaSessionAction action,
                 const base::string16& accessible_name) {
    set_tag(static_cast<int>(action));
    SetTooltipText(accessible_name);
    views::SetImageFromVectorIcon(
        this, GetVectorIconForMediaAction(action), icon_size_,
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconPrimary,
            AshColorProvider::AshColorMode::kDark));
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return std::make_unique<views::InkDropHighlight>(gfx::SizeF(size()),
                                                     GetInkDropBaseColor());
  }

 private:
  int const icon_size_;

  DISALLOW_COPY_AND_ASSIGN(MediaActionButton);
};

}  // namespace

const char LockScreenMediaControlsView::kMediaControlsHideHistogramName[] =
    "Media.LockScreenControls.Hide";

const char LockScreenMediaControlsView::kMediaControlsShownHistogramName[] =
    "Media.LockScreenControls.Shown";

const char
    LockScreenMediaControlsView::kMediaControlsUserActionHistogramName[] =
        "Media.LockScreenControls.UserAction";

LockScreenMediaControlsView::Callbacks::Callbacks() = default;

LockScreenMediaControlsView::Callbacks::~Callbacks() = default;

LockScreenMediaControlsView::LockScreenMediaControlsView(
    const Callbacks& callbacks)
    : hide_controls_timer_(new base::OneShotTimer()),
      hide_artwork_timer_(new base::OneShotTimer()),
      media_controls_enabled_(callbacks.media_controls_enabled),
      hide_media_controls_(callbacks.hide_media_controls),
      show_media_controls_(callbacks.show_media_controls) {
  DCHECK(callbacks.media_controls_enabled);
  DCHECK(callbacks.hide_media_controls);
  DCHECK(callbacks.show_media_controls);

  // Media controls should observe power events and handle the case of being
  // created in suspended state.
  base::PowerMonitor::AddObserver(this);
  if (base::PowerMonitor::IsProcessSuspended()) {
    // Post OnSuspend call to run after LockContentsView is initialized.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&LockScreenMediaControlsView::OnSuspend,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  // Media controls have not been dismissed initially.
  Shell::Get()->media_controller()->SetMediaControlsDismissed(false);

  middle_spacing_ = std::make_unique<NonAccessibleView>();
  middle_spacing_->set_owned_by_client();

  set_notify_enter_exit_on_child(true);

  contents_view_ = AddChildView(std::make_unique<views::View>());
  contents_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kMediaControlsInsets));
  contents_view_->SetBackground(views::CreateRoundedRectBackground(
      AshColorProvider::Get()->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent80,
          AshColorProvider::AshColorMode::kDark),
      kMediaControlsCornerRadius));

  contents_view_->SetPaintToLayer();  // Needed for opacity animation.
  contents_view_->layer()->SetFillsBoundsOpaquely(false);

  // |header_row_| contains the app icon and source title of the current media
  // session. It also contains the close button.
  header_row_ = contents_view_->AddChildView(
      std::make_unique<MediaControlsHeaderView>(base::BindOnce(
          &LockScreenMediaControlsView::Dismiss, base::Unretained(this))));

  // |artwork_row| contains the session artwork, artist and track info.
  auto artwork_row = std::make_unique<NonAccessibleView>();
  artwork_row->SetPreferredSize(kArtworkRowPreferredSize);
  auto* artwork_row_layout =
      artwork_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kArtworkRowInsets,
          kArtworkRowPadding));
  artwork_row_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  artwork_row_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  auto session_artwork = std::make_unique<views::ImageView>();
  session_artwork->SetPreferredSize(
      gfx::Size(kDesiredArtworkSize, kDesiredArtworkSize));
  session_artwork_ = artwork_row->AddChildView(std::move(session_artwork));
  session_artwork_->SetVisible(false);

  // |track_column| contains the title and artist labels of the current media
  // session.
  auto track_column = std::make_unique<NonAccessibleView>();
  auto* track_column_layout =
      track_column->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  track_column_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();

  auto title_label = std::make_unique<views::Label>();
  title_label->SetFontList(base_font_list.Derive(
      2, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::BOLD));
  title_label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextPrimary,
      AshColorProvider::AshColorMode::kDark));
  title_label->SetAutoColorReadabilityEnabled(false);
  title_label->SetElideBehavior(gfx::ELIDE_TAIL);
  title_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title_label_ = track_column->AddChildView(std::move(title_label));

  auto artist_label = std::make_unique<views::Label>();
  artist_label->SetFontList(base_font_list.Derive(
      0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::LIGHT));
  artist_label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextSecondary,
      AshColorProvider::AshColorMode::kDark));
  artist_label->SetAutoColorReadabilityEnabled(false);
  artist_label->SetElideBehavior(gfx::ELIDE_TAIL);
  artist_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  artist_label_ = track_column->AddChildView(std::move(artist_label));

  artwork_row->AddChildView(std::move(track_column));

  contents_view_->AddChildView(std::move(artwork_row));

  auto progress_view =
      std::make_unique<media_message_center::MediaControlsProgressView>(
          base::BindRepeating(&LockScreenMediaControlsView::SeekTo,
                              base::Unretained(this)));
  progress_view->SetForegroundColor(kProgressBarForeground);
  progress_view->SetBackgroundColor(kProgressBarBackground);
  progress_ = contents_view_->AddChildView(std::move(progress_view));

  // |button_row_| contains the buttons for controlling playback.
  auto button_row = std::make_unique<NonAccessibleView>();

  // left_control_group contains previous_track_button and seek_backward_button.
  auto left_control_group = std::make_unique<NonAccessibleView>();

  // right_control_group contains next_track_button and seek_forward_button.
  auto right_control_group = std::make_unique<NonAccessibleView>();

  auto* button_row_layout =
      button_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kButtonRowInsets,
          kMediaButtonRowPadding));
  button_row_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  button_row_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  auto* left_control_group_layout =
      left_control_group->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kMediaButtonRowPadding));
  left_control_group_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  left_control_group_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);

  auto* right_control_group_layout =
      right_control_group->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kMediaButtonRowPadding));
  right_control_group_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  right_control_group_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  button_row->SetPreferredSize(kMediaControlsButtonRowSize);
  button_row_ = contents_view_->AddChildView(std::move(button_row));

  left_control_group->SetPreferredSize(kMediaButtonGroupSize);
  right_control_group->SetPreferredSize(kMediaButtonGroupSize);

  media_action_buttons_.push_back(
      left_control_group->AddChildView(std::make_unique<MediaActionButton>(
          this, kMediaControlsIconSize, MediaSessionAction::kPreviousTrack,
          l10n_util::GetStringUTF16(
              IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACTION_PREVIOUS_TRACK))));

  media_action_buttons_.push_back(
      left_control_group->AddChildView(std::make_unique<MediaActionButton>(
          this, kMediaControlsIconSize, MediaSessionAction::kSeekBackward,
          l10n_util::GetStringUTF16(
              IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACTION_SEEK_BACKWARD))));

  button_row_->AddChildView(std::move(left_control_group));

  // |play_pause_button_| toggles playback. If the current media is playing, the
  // tag will be |MediaSessionAction::kPause|. If the current media is paused,
  // the tag will be |MediaSessionAction::kPlay|.
  play_pause_button_ =
      button_row_->AddChildView(std::make_unique<MediaActionButton>(
          this, kPlayPauseIconSize, MediaSessionAction::kPause,
          l10n_util::GetStringUTF16(
              IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACTION_PAUSE)));
  media_action_buttons_.push_back(play_pause_button_);

  media_action_buttons_.push_back(
      right_control_group->AddChildView(std::make_unique<MediaActionButton>(
          this, kMediaControlsIconSize, MediaSessionAction::kSeekForward,
          l10n_util::GetStringUTF16(
              IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACTION_SEEK_FORWARD))));

  media_action_buttons_.push_back(
      right_control_group->AddChildView(std::make_unique<MediaActionButton>(
          this, kMediaControlsIconSize, MediaSessionAction::kNextTrack,
          l10n_util::GetStringUTF16(
              IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACTION_NEXT_TRACK))));

  button_row_->AddChildView(std::move(right_control_group));

  // Set child view data to default values initially, until the media controller
  // observers are triggered by a change in media session state.
  MediaSessionMetadataChanged(base::nullopt);
  MediaSessionPositionChanged(base::nullopt);
  MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType::kSourceIcon, SkBitmap());
  SetArtwork(base::nullopt);

  // |service| can be null in tests.
  media_session::mojom::MediaSessionService* service =
      Shell::Get()->shell_delegate()->GetMediaSessionService();
  if (!service)
    return;

  // Connect to the MediaControllerManager and create a MediaController that
  // controls the active session so we can observe it.
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote;
  service->BindMediaControllerManager(
      controller_manager_remote.BindNewPipeAndPassReceiver());
  controller_manager_remote->CreateActiveMediaController(
      media_controller_remote_.BindNewPipeAndPassReceiver());

  // Observe the active media controller for changes.
  media_controller_remote_->AddObserver(
      observer_receiver_.BindNewPipeAndPassRemote());

  media_controller_remote_->ObserveImages(
      media_session::mojom::MediaSessionImageType::kArtwork,
      kMinimumArtworkSize, kDesiredArtworkSize,
      artwork_observer_receiver_.BindNewPipeAndPassRemote());

  media_controller_remote_->ObserveImages(
      media_session::mojom::MediaSessionImageType::kSourceIcon,
      kMinimumSourceIconSize, kDesiredSourceIconSize,
      icon_observer_receiver_.BindNewPipeAndPassRemote());
}

LockScreenMediaControlsView::~LockScreenMediaControlsView() {
  // If the screen is now unlocked and we were not hidden for another reason
  // then we are being hidden because the device is now unlocked.
  if (shown_ == Shown::kShown) {
    if (!hide_reason_ && !Shell::Get()->session_controller()->IsScreenLocked())
      hide_reason_ = HideReason::kUnlocked;

    base::UmaHistogramEnumeration(kMediaControlsHideHistogramName,
                                  *hide_reason_);
  }

  base::PowerMonitor::RemoveObserver(this);
}

const char* LockScreenMediaControlsView::GetClassName() const {
  return kLockScreenMediaControlsViewName;
}

gfx::Size LockScreenMediaControlsView::CalculatePreferredSize() const {
  return contents_view_->GetPreferredSize();
}

void LockScreenMediaControlsView::Layout() {
  contents_view_->SetBoundsRect(GetContentsBounds());
}

void LockScreenMediaControlsView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kListItem;
  node_data->AddStringAttribute(
      ax::mojom::StringAttribute::kRoleDescription,
      l10n_util::GetStringUTF8(
          IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACCESSIBLE_NAME));

  if (!accessible_name_.empty())
    node_data->SetName(accessible_name_);
}

void LockScreenMediaControlsView::OnMouseEntered(const ui::MouseEvent& event) {
  if (is_in_drag_ || contents_view_->layer()->GetAnimator()->is_animating())
    return;

  header_row_->SetCloseButtonVisibility(true);
}

void LockScreenMediaControlsView::OnMouseExited(const ui::MouseEvent& event) {
  if (is_in_drag_ || contents_view_->layer()->GetAnimator()->is_animating())
    return;

  header_row_->SetCloseButtonVisibility(false);
}

views::View* LockScreenMediaControlsView::GetMiddleSpacingView() {
  return middle_spacing_.get();
}

void LockScreenMediaControlsView::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  if (hide_controls_timer_->IsRunning())
    return;

  // If the controls are disabled then don't show the controls.
  if (!media_controls_enabled_.Run()) {
    SetShown(Shown::kNotShownControlsDisabled);
    return;
  }

  // If there is no session to show then don't show the controls.
  if (!session_info) {
    SetShown(Shown::kNotShownNoSession);
    return;
  }

  // If the session is marked as sensitive then don't show the controls.
  if (session_info->is_sensitive && !IsDrawn()) {
    SetShown(Shown::kNotShownSessionSensitive);
    return;
  }

  bool is_paused = session_info->playback_state ==
                   media_session::mojom::MediaPlaybackState::kPaused;

  // If the screen is locked while media is paused then don't show the controls.
  if (is_paused && !IsDrawn()) {
    SetShown(Shown::kNotShownSessionPaused);
    return;
  }

  if (!IsDrawn())
    SetShown(Shown::kShown);

  SetIsPlaying(!is_paused);
}

void LockScreenMediaControlsView::MediaSessionMetadataChanged(
    const base::Optional<media_session::MediaMetadata>& metadata) {
  if (hide_controls_timer_->IsRunning())
    return;

  media_session::MediaMetadata session_metadata =
      metadata.value_or(media_session::MediaMetadata());
  base::string16 source_title =
      session_metadata.source_title.empty()
          ? message_center::MessageCenter::Get()->GetSystemNotificationAppName()
          : session_metadata.source_title;
  header_row_->SetAppName(source_title);

  title_label_->SetText(session_metadata.title);
  artist_label_->SetText(session_metadata.artist);

  accessible_name_ =
      media_message_center::GetAccessibleNameFromMetadata(session_metadata);
}

void LockScreenMediaControlsView::MediaSessionActionsChanged(
    const std::vector<MediaSessionAction>& actions) {
  if (hide_controls_timer_->IsRunning())
    return;

  enabled_actions_ =
      base::flat_set<MediaSessionAction>(actions.begin(), actions.end());

  UpdateActionButtonsVisibility();
}

void LockScreenMediaControlsView::MediaSessionChanged(
    const base::Optional<base::UnguessableToken>& request_id) {
  if (!media_session_id_.has_value()) {
    media_session_id_ = request_id;
    return;
  }

  // If |media_session_id_| resumed while waiting, don't hide the controls.
  if (hide_controls_timer_->IsRunning() && request_id == media_session_id_)
    hide_controls_timer_->Stop();

  // If this session is different than the previous one, wait to see if the
  // previous one resumes before hiding the controls.
  if (request_id == media_session_id_)
    return;

  hide_controls_timer_->Start(
      FROM_HERE, kNextMediaDelay,
      base::BindOnce(&LockScreenMediaControlsView::Hide, base::Unretained(this),
                     HideReason::kSessionChanged));
}

void LockScreenMediaControlsView::MediaSessionPositionChanged(
    const base::Optional<media_session::MediaPosition>& position) {
  if (hide_controls_timer_->IsRunning())
    return;

  position_ = position;

  if (!position.has_value()) {
    if (progress_->GetVisible()) {
      progress_->SetVisible(false);
      Layout();
    }
    return;
  }

  progress_->UpdateProgress(*position);

  if (!progress_->GetVisible()) {
    progress_->SetVisible(true);
    Layout();
  }
}

void LockScreenMediaControlsView::MediaControllerImageChanged(
    media_session::mojom::MediaSessionImageType type,
    const SkBitmap& bitmap) {
  if (hide_controls_timer_->IsRunning())
    return;

  // Convert the bitmap to kN32_SkColorType if necessary.
  SkBitmap converted_bitmap;
  if (bitmap.colorType() == kN32_SkColorType) {
    converted_bitmap = bitmap;
  } else {
    SkImageInfo info = bitmap.info().makeColorType(kN32_SkColorType);
    if (converted_bitmap.tryAllocPixels(info)) {
      bitmap.readPixels(info, converted_bitmap.getPixels(),
                        converted_bitmap.rowBytes(), 0, 0);
    }
  }

  switch (type) {
    case media_session::mojom::MediaSessionImageType::kArtwork: {
      base::Optional<gfx::ImageSkia> session_artwork;
      if (!converted_bitmap.empty())
        session_artwork = gfx::ImageSkia::CreateFrom1xBitmap(converted_bitmap);
      SetArtwork(session_artwork);
      break;
    }
    case media_session::mojom::MediaSessionImageType::kSourceIcon: {
      gfx::ImageSkia session_icon =
          gfx::ImageSkia::CreateFrom1xBitmap(converted_bitmap);
      if (session_icon.isNull()) {
        session_icon =
            gfx::CreateVectorIcon(message_center::kProductIcon,
                                  kDesiredSourceIconSize, gfx::kChromeIconGrey);
      }
      header_row_->SetAppIcon(session_icon);
    }
  }
}

void LockScreenMediaControlsView::OnImplicitAnimationsCompleted() {
  Dismiss();
}

void LockScreenMediaControlsView::ButtonPressed(views::Button* sender,
                                                const ui::Event& event) {
  if (!base::Contains(enabled_actions_,
                      media_message_center::GetActionFromButtonTag(*sender)) ||
      !media_session_id_.has_value()) {
    return;
  }

  auto action = media_message_center::GetActionFromButtonTag(*sender);

  base::UmaHistogramEnumeration(kMediaControlsUserActionHistogramName, action);

  media_session::PerformMediaSessionAction(action, media_controller_remote_);
}

void LockScreenMediaControlsView::OnGestureEvent(ui::GestureEvent* event) {
  gfx::Point point_in_screen = event->location();
  ConvertPointToScreen(this, &point_in_screen);

  switch (event->type()) {
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_GESTURE_SCROLL_BEGIN: {
      if (is_in_drag_)
        break;

      initial_drag_point_ = point_in_screen;
      is_in_drag_ = true;
      event->SetHandled();
      break;
    }
    case ui::ET_GESTURE_SCROLL_UPDATE: {
      last_fling_velocity_ = event->details().scroll_x();
      UpdateDrag(point_in_screen);
      event->SetHandled();
      break;
    }
    case ui::ET_GESTURE_END: {
      if (!is_in_drag_)
        break;

      EndDrag();
      event->SetHandled();
      break;
    }
    default:
      break;
  }
}

void LockScreenMediaControlsView::OnSuspend() {
  Hide(HideReason::kDeviceSleep);
}

void LockScreenMediaControlsView::FlushForTesting() {
  media_controller_remote_.FlushForTesting();
}

void LockScreenMediaControlsView::UpdateActionButtonsVisibility() {
  base::flat_set<MediaSessionAction> ignored_actions = {
      media_message_center::GetPlayPauseIgnoredAction(
          media_message_center::GetActionFromButtonTag(*play_pause_button_))};

  base::flat_set<MediaSessionAction> visible_actions =
      media_message_center::GetTopVisibleActions(enabled_actions_,
                                                 ignored_actions, kMaxActions);

  bool should_invalidate = false;
  for (views::Button* action_button : media_action_buttons_) {
    bool should_show = base::Contains(
        visible_actions,
        media_message_center::GetActionFromButtonTag(*action_button));
    should_invalidate |= should_show != action_button->GetVisible();

    action_button->SetVisible(should_show);
  }

  if (should_invalidate)
    button_row_->InvalidateLayout();
}

void LockScreenMediaControlsView::SetIsPlaying(bool playing) {
  if (playing) {
    play_pause_button_->SetAction(
        MediaSessionAction::kPause,
        l10n_util::GetStringUTF16(
            IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACTION_PAUSE));
  } else {
    play_pause_button_->SetAction(
        MediaSessionAction::kPlay,
        l10n_util::GetStringUTF16(
            IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_ACTION_PLAY));
  }

  UpdateActionButtonsVisibility();
}

void LockScreenMediaControlsView::SeekTo(double seek_progress) {
  DCHECK(position_.has_value());

  media_controller_remote_->SeekTo(seek_progress * position_->duration());

  base::UmaHistogramEnumeration(kMediaControlsUserActionHistogramName,
                                MediaSessionAction::kSeekTo);
}

void LockScreenMediaControlsView::Hide(HideReason reason) {
  if (!hide_reason_ && GetVisible())
    hide_reason_ = reason;

  hide_media_controls_.Run();
}

void LockScreenMediaControlsView::HideArtwork() {
  session_artwork_->SetVisible(false);
  session_artwork_->SetImage(nullptr);
  session_artwork_->InvalidateLayout();
}

void LockScreenMediaControlsView::SetShown(Shown shown) {
  if (shown_ == shown)
    return;

  shown_ = shown;

  base::UmaHistogramEnumeration(kMediaControlsShownHistogramName, shown);

  if (shown == Shown::kShown) {
    show_media_controls_.Run();
  } else {
    hide_media_controls_.Run();
  }
}

void LockScreenMediaControlsView::Dismiss() {
  media_controller_remote_->Stop();

  base::UmaHistogramEnumeration(kMediaControlsUserActionHistogramName,
                                MediaSessionAction::kStop);

  Hide(HideReason::kDismissedByUser);
}

void LockScreenMediaControlsView::SetArtwork(
    base::Optional<gfx::ImageSkia> img) {
  if (!img.has_value()) {
    if (!session_artwork_->GetVisible() || hide_artwork_timer_->IsRunning())
      return;

    hide_artwork_timer_->Start(
        FROM_HERE, kNextMediaDelay,
        base::BindOnce(&LockScreenMediaControlsView::HideArtwork,
                       base::Unretained(this)));
    return;
  }

  if (hide_artwork_timer_->IsRunning())
    hide_artwork_timer_->Stop();

  session_artwork_->SetVisible(true);
  session_artwork_->SetImageSize(
      ScaleSizeToFitView(img->size(), session_artwork_->GetPreferredSize()));
  session_artwork_->SetImage(*img);

  Layout();
  session_artwork_->SetClipPath(GetArtworkClipPath());
}

SkPath LockScreenMediaControlsView::GetArtworkClipPath() const {
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(session_artwork_->GetImageBounds()),
                    kArtworkCornerRadius, kArtworkCornerRadius);
  return path;
}

void LockScreenMediaControlsView::UpdateDrag(
    const gfx::Point& location_in_screen) {
  is_in_drag_ = true;
  int drag_delta = location_in_screen.x() - initial_drag_point_.x();

  // Don't let the user drag |contents_view_| below the view area.
  if (contents_view_->bounds().x() + drag_delta <= GetLocalBounds().x()) {
    return;
  }

  gfx::Transform transform;
  transform.Translate(drag_delta, 0);
  contents_view_->layer()->SetTransform(transform);
  UpdateOpacity();
}

void LockScreenMediaControlsView::EndDrag() {
  is_in_drag_ = false;

  int threshold = GetBoundsInScreen().right() - kDistanceDismissalThreshold;

  // If the user releases the drag with velocity over the threshold or drags
  // |contents_view_| past the distance threshold, dismiss the controls.
  if (last_fling_velocity_ >= kDragVelocityThreshold ||
      (contents_view_->GetBoundsInScreen().x() >= threshold &&
       last_fling_velocity_ < 1)) {
    RunHideControlsAnimation();
    return;
  }

  RunResetControlsAnimation();
}

void LockScreenMediaControlsView::UpdateOpacity() {
  float progress =
      GetBoundsInScreen().x() /
      static_cast<float>(contents_view_->GetBoundsInScreen().right());
  contents_view_->layer()->SetOpacity(progress);
}

void LockScreenMediaControlsView::RunHideControlsAnimation() {
  ui::ScopedLayerAnimationSettings animation(
      contents_view_->layer()->GetAnimator());
  animation.AddObserver(this);
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animation.SetTransitionDuration(kAnimationDuration);

  gfx::Transform transform;
  transform.Translate(GetBoundsInScreen().right(), 0);
  contents_view_->layer()->SetTransform(transform);
  contents_view_->layer()->SetOpacity(0);
}

void LockScreenMediaControlsView::RunResetControlsAnimation() {
  ui::ScopedLayerAnimationSettings animation(
      contents_view_->layer()->GetAnimator());
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animation.SetTransitionDuration(kAnimationDuration);

  contents_view_->layer()->SetTransform(gfx::Transform());
  contents_view_->layer()->SetOpacity(1);
}

}  // namespace ash
