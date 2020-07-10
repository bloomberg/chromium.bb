// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_notification_service.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/global_media_controls/media_dialog_delegate.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_impl.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_observer.h"
#include "chrome/browser/ui/global_media_controls/overlay_media_notification.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
#include "components/media_message_center/media_session_notification_item.h"
#include "content/public/browser/media_session.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

constexpr base::TimeDelta kInactiveTimerDelay =
    base::TimeDelta::FromMinutes(60);

// Here we check to see if the WebContents is focused. Note that since Session
// is a WebContentsObserver, we could in theory listen for
// |OnWebContentsFocused()| and |OnWebContentsLostFocus()|. However, this won't
// actually work since focusing the MediaDialogView causes the WebContents to
// "lose focus", so we'd never be focused.
bool IsWebContentsFocused(content::WebContents* web_contents) {
  DCHECK(web_contents);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return false;

  // If the given WebContents is not in the focused window, then it's not
  // focused. Note that we know a Browser is focused because otherwise the user
  // could not interact with the MediaDialogView.
  if (BrowserList::GetInstance()->GetLastActive() != browser)
    return false;

  return browser->tab_strip_model()->GetActiveWebContents() == web_contents;
}

}  // anonymous namespace

MediaNotificationService::Session::Session(
    MediaNotificationService* owner,
    const std::string& id,
    std::unique_ptr<media_message_center::MediaSessionNotificationItem> item,
    content::WebContents* web_contents,
    mojo::Remote<media_session::mojom::MediaController> controller)
    : content::WebContentsObserver(web_contents),
      owner_(owner),
      id_(id),
      item_(std::move(item)) {
  DCHECK(owner_);
  DCHECK(item_);

  SetController(std::move(controller));
}

MediaNotificationService::Session::~Session() {
  // If we've been marked inactive, then we've already recorded inactivity as
  // the dismiss reason.
  if (is_marked_inactive_)
    return;

  RecordDismissReason(dismiss_reason_.value_or(
      GlobalMediaControlsDismissReason::kMediaSessionStopped));
}

void MediaNotificationService::Session::WebContentsDestroyed() {
  // If the WebContents is destroyed, then we should just remove the item
  // instead of freezing it.
  set_dismiss_reason(GlobalMediaControlsDismissReason::kTabClosed);
  owner_->RemoveItem(id_);
}

void MediaNotificationService::Session::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  is_playing_ =
      session_info && session_info->playback_state ==
                          media_session::mojom::MediaPlaybackState::kPlaying;

  // If we've started playing, we don't want the inactive timer to be running.
  if (is_playing_) {
    if (inactive_timer_.IsRunning() || is_marked_inactive_) {
      MarkActiveIfNecessary();
      RecordInteractionDelayAfterPause();
      inactive_timer_.Stop();
    }
    return;
  }

  // If we're in an overlay, then we don't want to count the session as
  // inactive.
  // TODO(https://crbug.com/1032841): This means we won't record interaction
  // delays. Consider changing to record them.
  if (is_in_overlay_)
    return;

  // If the timer is already running, we don't need to do anything.
  if (inactive_timer_.IsRunning())
    return;

  last_interaction_time_ = base::TimeTicks::Now();
  StartInactiveTimer();
}

void MediaNotificationService::Session::MediaSessionPositionChanged(
    const base::Optional<media_session::MediaPosition>& position) {
  OnSessionInteractedWith();
}

void MediaNotificationService::Session::SetController(
    mojo::Remote<media_session::mojom::MediaController> controller) {
  if (controller.is_bound()) {
    observer_receiver_.reset();
    controller->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());
  }
}

void MediaNotificationService::Session::set_dismiss_reason(
    GlobalMediaControlsDismissReason reason) {
  DCHECK(!dismiss_reason_.has_value());
  dismiss_reason_ = reason;
}

void MediaNotificationService::Session::OnSessionInteractedWith() {
  // If we're not currently tracking inactive time, then no action is needed.
  if (!inactive_timer_.IsRunning() && !is_marked_inactive_)
    return;

  MarkActiveIfNecessary();

  RecordInteractionDelayAfterPause();
  last_interaction_time_ = base::TimeTicks::Now();

  // Otherwise, reset the timer.
  inactive_timer_.Stop();
  StartInactiveTimer();
}

void MediaNotificationService::Session::OnSessionOverlayStateChanged(
    bool is_in_overlay) {
  is_in_overlay_ = is_in_overlay;

  if (is_in_overlay_) {
    // If we enter an overlay, then we don't want the session to be marked
    // inactive.
    if (inactive_timer_.IsRunning()) {
      RecordInteractionDelayAfterPause();
      inactive_timer_.Stop();
    }
  } else if (!is_playing_ && !inactive_timer_.IsRunning()) {
    // If we exit an overlay and the session is paused, then the session is
    // inactive.
    StartInactiveTimer();
  }
}

// static
void MediaNotificationService::Session::RecordDismissReason(
    GlobalMediaControlsDismissReason reason) {
  base::UmaHistogramEnumeration("Media.GlobalMediaControls.DismissReason",
                                reason);
}

void MediaNotificationService::Session::StartInactiveTimer() {
  DCHECK(!inactive_timer_.IsRunning());

  // Using |base::Unretained()| here is okay since |this| owns
  // |inactive_timer_|.
  inactive_timer_.Start(
      FROM_HERE, kInactiveTimerDelay,
      base::BindOnce(&MediaNotificationService::Session::OnInactiveTimerFired,
                     base::Unretained(this)));
}

void MediaNotificationService::Session::OnInactiveTimerFired() {
  // Overlay notifications should never be marked as inactive.
  DCHECK(!is_in_overlay_);

  // If the session has been paused and inactive for long enough, then mark it
  // as inactive.
  is_marked_inactive_ = true;
  RecordDismissReason(GlobalMediaControlsDismissReason::kInactiveTimeout);
  owner_->OnSessionBecameInactive(id_);
}

void MediaNotificationService::Session::RecordInteractionDelayAfterPause() {
  base::TimeDelta time_since_last_interaction =
      base::TimeTicks::Now() - last_interaction_time_;
  base::UmaHistogramCustomTimes(
      "Media.GlobalMediaControls.InteractionDelayAfterPause",
      time_since_last_interaction, base::TimeDelta::FromMinutes(1),
      base::TimeDelta::FromDays(1), 100);
}

void MediaNotificationService::Session::MarkActiveIfNecessary() {
  if (!is_marked_inactive_)
    return;
  is_marked_inactive_ = false;

  owner_->OnSessionBecameActive(id_);
}

MediaNotificationService::MediaNotificationService(
    Profile* profile,
    service_manager::Connector* connector)
    : connector_(connector), overlay_media_notifications_manager_(this) {
  if (base::FeatureList::IsEnabled(media::kGlobalMediaControlsForCast) &&
      media_router::MediaRouterEnabled(profile)) {
    cast_notification_provider_ =
        std::make_unique<CastMediaNotificationProvider>(
            profile, this,
            base::BindRepeating(
                &MediaNotificationService::OnCastNotificationsChanged,
                base::Unretained(this)));
  }

  // |connector_| can be null in tests.
  if (!connector_)
    return;

  const base::UnguessableToken& source_id =
      content::MediaSession::GetSourceId(profile);

  // Connect to the controller manager so we can create media controllers for
  // media sessions.
  connector_->Connect(media_session::mojom::kServiceName,
                      controller_manager_remote_.BindNewPipeAndPassReceiver());

  // Connect to receive audio focus events.
  connector_->Connect(media_session::mojom::kServiceName,
                      audio_focus_remote_.BindNewPipeAndPassReceiver());
  audio_focus_remote_->AddSourceObserver(
      source_id, audio_focus_observer_receiver_.BindNewPipeAndPassRemote());

  audio_focus_remote_->GetSourceFocusRequests(
      source_id,
      base::BindOnce(&MediaNotificationService::OnReceivedAudioFocusRequests,
                     weak_ptr_factory_.GetWeakPtr()));
}

MediaNotificationService::~MediaNotificationService() {
  for (auto container_pair : observed_containers_)
    container_pair.second->RemoveObserver(this);
}

void MediaNotificationService::AddObserver(
    MediaNotificationServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaNotificationService::RemoveObserver(
    MediaNotificationServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MediaNotificationService::OnFocusGained(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  const std::string id = session->request_id->ToString();

  // If we have an existing unfrozen item then this is a duplicate call and
  // we should ignore it.
  auto it = sessions_.find(id);
  if (it != sessions_.end() && !it->second.item()->frozen())
    return;

  mojo::Remote<media_session::mojom::MediaController> item_controller;
  mojo::Remote<media_session::mojom::MediaController> session_controller;

  // |controller_manager_remote_| may be null in tests where connector is
  // unavailable.
  if (controller_manager_remote_) {
    controller_manager_remote_->CreateMediaControllerForSession(
        item_controller.BindNewPipeAndPassReceiver(), *session->request_id);
    controller_manager_remote_->CreateMediaControllerForSession(
        session_controller.BindNewPipeAndPassReceiver(), *session->request_id);
  }

  if (it != sessions_.end()) {
    // If the notification was previously frozen then we should reset the
    // controller because the mojo pipe would have been reset.
    it->second.SetController(std::move(session_controller));
    it->second.item()->SetController(std::move(item_controller),
                                     std::move(session->session_info));
    if (!base::Contains(dragged_out_session_ids_, id))
      active_controllable_session_ids_.insert(id);
    frozen_session_ids_.erase(id);
    for (auto& observer : observers_)
      observer.OnNotificationListChanged();
  } else {
    sessions_.emplace(
        std::piecewise_construct, std::forward_as_tuple(id),
        std::forward_as_tuple(
            this, id,
            std::make_unique<
                media_message_center::MediaSessionNotificationItem>(
                this, id, session->source_name.value_or(std::string()),
                std::move(item_controller), std::move(session->session_info)),
            content::MediaSession::GetWebContentsFromRequestId(
                *session->request_id),
            std::move(session_controller)));
  }
}

void MediaNotificationService::OnFocusLost(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  const std::string id = session->request_id->ToString();

  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return;

  it->second.item()->Freeze();
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.insert(id);
  for (auto& observer : observers_)
    observer.OnNotificationListChanged();
}

void MediaNotificationService::ShowNotification(const std::string& id) {
  // If the notification is currently hidden because it's inactive or because
  // it's in an overlay notification, then do nothing.
  if (base::Contains(dragged_out_session_ids_, id) ||
      base::Contains(inactive_session_ids_, id)) {
    return;
  }

  active_controllable_session_ids_.insert(id);

  for (auto& observer : observers_)
    observer.OnNotificationListChanged();

  if (!dialog_delegate_)
    return;

  base::WeakPtr<media_message_center::MediaNotificationItem> item =
      GetNotificationItem(id);
  MediaNotificationContainerImpl* container =
      dialog_delegate_->ShowMediaSession(id, item);

  // Observe the container for dismissal.
  if (container) {
    container->AddObserver(this);
    observed_containers_[id] = container;
  }
}

void MediaNotificationService::HideNotification(const std::string& id) {
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.erase(id);

  if (base::Contains(dragged_out_session_ids_, id)) {
    overlay_media_notifications_manager_.CloseOverlayNotification(id);
    dragged_out_session_ids_.erase(id);
  }

  for (auto& observer : observers_)
    observer.OnNotificationListChanged();

  if (!dialog_delegate_)
    return;

  dialog_delegate_->HideMediaSession(id);
}

scoped_refptr<base::SequencedTaskRunner>
MediaNotificationService::GetTaskRunner() const {
  return nullptr;
}

void MediaNotificationService::RemoveItem(const std::string& id) {
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.erase(id);
  inactive_session_ids_.erase(id);

  if (base::Contains(dragged_out_session_ids_, id)) {
    overlay_media_notifications_manager_.CloseOverlayNotification(id);
    dragged_out_session_ids_.erase(id);
  }

  sessions_.erase(id);

  for (auto& observer : observers_)
    observer.OnNotificationListChanged();
}

void MediaNotificationService::LogMediaSessionActionButtonPressed(
    const std::string& id) {
  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return;

  content::WebContents* web_contents = it->second.web_contents();
  if (!web_contents)
    return;

  base::UmaHistogramBoolean("Media.GlobalMediaControls.UserActionFocus",
                            IsWebContentsFocused(web_contents));
}

void MediaNotificationService::OnContainerClicked(const std::string& id) {
  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return;

  it->second.OnSessionInteractedWith();

  content::WebContents* web_contents = it->second.web_contents();
  if (!web_contents)
    return;

  content::WebContentsDelegate* delegate = web_contents->GetDelegate();
  if (!delegate)
    return;

  delegate->ActivateContents(web_contents);
}

void MediaNotificationService::OnContainerDismissed(const std::string& id) {
  // If the notification is dragged out, then dismissing should just close the
  // overlay notification.
  if (base::Contains(dragged_out_session_ids_, id)) {
    overlay_media_notifications_manager_.CloseOverlayNotification(id);
    return;
  }

  auto it = sessions_.find(id);
  if (it == sessions_.end()) {
    if (!cast_notification_provider_)
      return;

    base::WeakPtr<media_message_center::MediaNotificationItem> cast_item =
        cast_notification_provider_->GetNotificationItem(id);
    if (cast_item)
      cast_item->Dismiss();

    return;
  }

  it->second.set_dismiss_reason(
      GlobalMediaControlsDismissReason::kUserDismissedNotification);
  it->second.item()->Dismiss();
}

void MediaNotificationService::OnContainerDestroyed(const std::string& id) {
  auto iter = observed_containers_.find(id);
  DCHECK(iter != observed_containers_.end());

  iter->second->RemoveObserver(this);
  observed_containers_.erase(iter);
}

void MediaNotificationService::OnContainerDraggedOut(const std::string& id,
                                                     gfx::Rect bounds) {
  // If the session has been destroyed, no action is needed.
  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return;

  // Inform the Session that it's in an overlay so should not timeout as
  // inactive.
  it->second.OnSessionOverlayStateChanged(/*is_in_overlay=*/true);

  if (!dialog_delegate_)
    return;

  std::unique_ptr<OverlayMediaNotification> overlay_notification =
      dialog_delegate_->PopOut(id, bounds);
  if (!overlay_notification)
    return;

  overlay_media_notifications_manager_.ShowOverlayNotification(
      id, std::move(overlay_notification));
  active_controllable_session_ids_.erase(id);
  dragged_out_session_ids_.insert(id);

  for (auto& observer : observers_)
    observer.OnNotificationListChanged();
}

void MediaNotificationService::Shutdown() {
  // |cast_notification_provider_| depends on MediaRouter, which is another
  // keyed service.
  cast_notification_provider_.reset();
}

void MediaNotificationService::OnOverlayNotificationClosed(
    const std::string& id) {
  // If the session has been destroyed, no action is needed.
  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return;

  it->second.OnSessionOverlayStateChanged(/*is_in_overlay=*/false);

  // Since the overlay is closing, we no longer need to observe the associated
  // container.
  auto observed_iter = observed_containers_.find(id);
  if (observed_iter != observed_containers_.end()) {
    observed_iter->second->RemoveObserver(this);
    observed_containers_.erase(observed_iter);
  }

  // Otherwise, if it's a non-frozen item, then it's now an active one.
  if (!base::Contains(frozen_session_ids_, id))
    active_controllable_session_ids_.insert(id);
  dragged_out_session_ids_.erase(id);

  for (auto& observer : observers_)
    observer.OnNotificationListChanged();

  // If there's a dialog currently open, then we should show the item in the
  // dialog.
  if (!dialog_delegate_)
    return;

  MediaNotificationContainerImpl* container =
      dialog_delegate_->ShowMediaSession(id, it->second.item()->GetWeakPtr());

  if (container) {
    container->AddObserver(this);
    observed_containers_[id] = container;
  }
}

void MediaNotificationService::OnCastNotificationsChanged() {
  for (auto& observer : observers_)
    observer.OnNotificationListChanged();
}

void MediaNotificationService::SetDialogDelegate(
    MediaDialogDelegate* delegate) {
  DCHECK(!delegate || !dialog_delegate_);
  dialog_delegate_ = delegate;

  for (auto& observer : observers_)
    observer.OnMediaDialogOpenedOrClosed();

  if (!dialog_delegate_)
    return;

  for (const std::string& id : active_controllable_session_ids_) {
    base::WeakPtr<media_message_center::MediaNotificationItem> item =
        GetNotificationItem(id);
    MediaNotificationContainerImpl* container =
        dialog_delegate_->ShowMediaSession(id, item);

    // Observe the container for dismissal.
    if (container) {
      container->AddObserver(this);
      observed_containers_[id] = container;
    }
  }

  media_message_center::RecordConcurrentNotificationCount(
      active_controllable_session_ids_.size());
}

bool MediaNotificationService::HasActiveNotifications() const {
  return !active_controllable_session_ids_.empty();
}

bool MediaNotificationService::HasFrozenNotifications() const {
  return !frozen_session_ids_.empty();
}

bool MediaNotificationService::HasOpenDialog() const {
  return !!dialog_delegate_;
}

void MediaNotificationService::OnSessionBecameActive(const std::string& id) {
  DCHECK(base::Contains(inactive_session_ids_, id));

  auto it = sessions_.find(id);
  DCHECK(it != sessions_.end());

  inactive_session_ids_.erase(id);

  if (it->second.item()->frozen())
    frozen_session_ids_.insert(id);
  else
    active_controllable_session_ids_.insert(id);

  for (auto& observer : observers_)
    observer.OnNotificationListChanged();

  // If there's a dialog currently open, then we should show the item in the
  // dialog.
  if (!dialog_delegate_)
    return;

  MediaNotificationContainerImpl* container =
      dialog_delegate_->ShowMediaSession(id, it->second.item()->GetWeakPtr());

  if (container) {
    container->AddObserver(this);
    observed_containers_[id] = container;
  }
}

void MediaNotificationService::OnSessionBecameInactive(const std::string& id) {
  // If this session is already marked inactive, then there's nothing to do.
  if (base::Contains(inactive_session_ids_, id))
    return;

  inactive_session_ids_.insert(id);

  HideNotification(id);
}

void MediaNotificationService::OnReceivedAudioFocusRequests(
    std::vector<media_session::mojom::AudioFocusRequestStatePtr> sessions) {
  for (auto& session : sessions)
    OnFocusGained(std::move(session));
}

base::WeakPtr<media_message_center::MediaNotificationItem>
MediaNotificationService::GetNotificationItem(const std::string& id) {
  auto it = sessions_.find(id);
  if (it != sessions_.end()) {
    return it->second.item()->GetWeakPtr();
  } else if (cast_notification_provider_) {
    return cast_notification_provider_->GetNotificationItem(id);
  }
  return nullptr;
}
