// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/renderer_webmediaplayer_delegate.h"

#include <stdint.h>

#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"

namespace {

void RecordAction(const base::UserMetricsAction& action) {
  content::RenderThread::Get()->RecordAction(action);
}

}  // namespace

namespace media {

RendererWebMediaPlayerDelegate::RendererWebMediaPlayerDelegate(
    content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      default_tick_clock_(new base::DefaultTickClock()),
      tick_clock_(default_tick_clock_.get()) {
  idle_cleanup_interval_ = base::TimeDelta::FromSeconds(5);
  idle_timeout_ = base::TimeDelta::FromSeconds(15);
}

RendererWebMediaPlayerDelegate::~RendererWebMediaPlayerDelegate() {}

int RendererWebMediaPlayerDelegate::AddObserver(Observer* observer) {
  return id_map_.Add(observer);
}

void RendererWebMediaPlayerDelegate::RemoveObserver(int delegate_id) {
  DCHECK(id_map_.Lookup(delegate_id));
  id_map_.Remove(delegate_id);
  RemoveIdleDelegate(delegate_id);
  playing_videos_.erase(delegate_id);
}

void RendererWebMediaPlayerDelegate::DidPlay(
    int delegate_id,
    bool has_video,
    bool has_audio,
    bool is_remote,
    MediaContentType media_content_type) {
  DCHECK(id_map_.Lookup(delegate_id));
  has_played_media_ = true;
  if (has_video && !is_remote)
    playing_videos_.insert(delegate_id);
  else
    playing_videos_.erase(delegate_id);
  RemoveIdleDelegate(delegate_id);
  Send(new MediaPlayerDelegateHostMsg_OnMediaPlaying(
      routing_id(), delegate_id, has_video, has_audio, is_remote,
      media_content_type));
}

void RendererWebMediaPlayerDelegate::DidPause(int delegate_id,
                                              bool reached_end_of_stream) {
  DCHECK(id_map_.Lookup(delegate_id));
  AddIdleDelegate(delegate_id);
  if (reached_end_of_stream)
    playing_videos_.erase(delegate_id);
  Send(new MediaPlayerDelegateHostMsg_OnMediaPaused(routing_id(), delegate_id,
                                                    reached_end_of_stream));
}

void RendererWebMediaPlayerDelegate::PlayerGone(int delegate_id) {
  DCHECK(id_map_.Lookup(delegate_id));
  RemoveIdleDelegate(delegate_id);
  playing_videos_.erase(delegate_id);
  Send(new MediaPlayerDelegateHostMsg_OnMediaDestroyed(routing_id(),
                                                       delegate_id));
}

bool RendererWebMediaPlayerDelegate::IsHidden() {
  return render_frame()->IsHidden();
}

bool RendererWebMediaPlayerDelegate::IsPlayingBackgroundVideo() {
  return is_playing_background_video_;
}

void RendererWebMediaPlayerDelegate::WasHidden() {
  for (IDMap<Observer>::iterator it(&id_map_); !it.IsAtEnd(); it.Advance())
    it.GetCurrentValue()->OnHidden();

  RecordAction(base::UserMetricsAction("Media.Hidden"));
}

void RendererWebMediaPlayerDelegate::WasShown() {
  SetIsPlayingBackgroundVideo(false);
  for (IDMap<Observer>::iterator it(&id_map_); !it.IsAtEnd(); it.Advance())
    it.GetCurrentValue()->OnShown();

  RecordAction(base::UserMetricsAction("Media.Shown"));
}

bool RendererWebMediaPlayerDelegate::OnMessageReceived(
    const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RendererWebMediaPlayerDelegate, msg)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_Pause, OnMediaDelegatePause)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_Play, OnMediaDelegatePlay)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_SuspendAllMediaPlayers,
                        OnMediaDelegateSuspendAllMediaPlayers)
    IPC_MESSAGE_HANDLER(MediaPlayerDelegateMsg_UpdateVolumeMultiplier,
                        OnMediaDelegateVolumeMultiplierUpdate)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RendererWebMediaPlayerDelegate::SetIdleCleanupParamsForTesting(
    base::TimeDelta idle_timeout,
    base::TickClock* tick_clock) {
  idle_cleanup_interval_ = base::TimeDelta();
  idle_timeout_ = idle_timeout;
  tick_clock_ = tick_clock;
}

void RendererWebMediaPlayerDelegate::OnMediaDelegatePause(int delegate_id) {
  Observer* observer = id_map_.Lookup(delegate_id);
  if (observer) {
    if (playing_videos_.find(delegate_id) != playing_videos_.end())
      SetIsPlayingBackgroundVideo(false);
    observer->OnPause();
  }

  RecordAction(base::UserMetricsAction("Media.Controls.RemotePause"));
}

void RendererWebMediaPlayerDelegate::OnMediaDelegatePlay(int delegate_id) {
  Observer* observer = id_map_.Lookup(delegate_id);
  if (observer) {
    if (playing_videos_.find(delegate_id) != playing_videos_.end())
      SetIsPlayingBackgroundVideo(IsHidden());
    observer->OnPlay();
  }

  RecordAction(base::UserMetricsAction("Media.Controls.RemotePlay"));
}

void RendererWebMediaPlayerDelegate::OnMediaDelegateSuspendAllMediaPlayers() {
  for (IDMap<Observer>::iterator it(&id_map_); !it.IsAtEnd(); it.Advance())
    it.GetCurrentValue()->OnSuspendRequested(true);
}

void RendererWebMediaPlayerDelegate::OnMediaDelegateVolumeMultiplierUpdate(
    int delegate_id,
    double multiplier) {
  Observer* observer = id_map_.Lookup(delegate_id);
  if (observer)
    observer->OnVolumeMultiplierUpdate(multiplier);
}

void RendererWebMediaPlayerDelegate::AddIdleDelegate(int delegate_id) {
  idle_delegate_map_[delegate_id] = tick_clock_->NowTicks();
  if (!idle_cleanup_timer_.IsRunning()) {
    idle_cleanup_timer_.Start(
        FROM_HERE, idle_cleanup_interval_, this,
        &RendererWebMediaPlayerDelegate::CleanupIdleDelegates);
  }
}

void RendererWebMediaPlayerDelegate::RemoveIdleDelegate(int delegate_id) {
  // To avoid invalidating the iterator, just mark the delegate for deletion
  // using a sentinel value of an empty TimeTicks.
  if (idle_cleanup_running_) {
    idle_delegate_map_[delegate_id] = base::TimeTicks();
    return;
  }

  idle_delegate_map_.erase(delegate_id);
  if (idle_delegate_map_.empty())
    idle_cleanup_timer_.Stop();
}

void RendererWebMediaPlayerDelegate::CleanupIdleDelegates() {
  // Iterate over the delegates and suspend the idle ones. Note: The call to
  // OnHidden() can trigger calls into RemoveIdleDelegate(), so for iterator
  // validity we set |idle_cleanup_running_| to true and defer deletions.
  base::AutoReset<bool> scoper(&idle_cleanup_running_, true);
  const base::TimeTicks now = tick_clock_->NowTicks();
  for (auto& idle_delegate_entry : idle_delegate_map_) {
    if (now - idle_delegate_entry.second > idle_timeout_) {
      id_map_.Lookup(idle_delegate_entry.first)->OnSuspendRequested(false);

      // Whether or not the player accepted the suspension, mark it for removal
      // from future polls to avoid running the timer forever.
      idle_delegate_entry.second = base::TimeTicks();
    }
  }

  // Take care of any removals that happened during the above iteration.
  for (auto it = idle_delegate_map_.begin(); it != idle_delegate_map_.end();) {
    if (it->second.is_null())
      it = idle_delegate_map_.erase(it);
    else
      ++it;
  }

  // Shutdown the timer if no delegates are left.
  if (idle_delegate_map_.empty())
    idle_cleanup_timer_.Stop();
}

void RendererWebMediaPlayerDelegate::SetIsPlayingBackgroundVideo(
    bool is_playing) {
  if (is_playing_background_video_ == is_playing) return;

// TODO(avayvod): This would be useful to collect on desktop too and express in
// actual media watch time vs. just elapsed time. See https://crbug.com/638726.
#if defined(OS_ANDROID)
  if (is_playing_background_video_) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Media.Android.BackgroundVideoTime",
        base::TimeTicks::Now() - background_video_playing_start_time_,
        base::TimeDelta::FromSeconds(7), base::TimeDelta::FromHours(10), 50);
    RecordAction(base::UserMetricsAction("Media.Session.BackgroundSuspend"));
  } else {
    background_video_playing_start_time_ = base::TimeTicks::Now();
    RecordAction(base::UserMetricsAction("Media.Session.BackgroundResume"));
  }
#endif  // OS_ANDROID

  is_playing_background_video_ = is_playing;
}

void RendererWebMediaPlayerDelegate::OnDestruct() {
  delete this;
}

}  // namespace media
