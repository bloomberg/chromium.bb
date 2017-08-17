// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_stream_monitor.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/invalidate_type.h"

namespace content {

namespace {

enum class ActionType { STARTING, STOPPING };
AudioStreamMonitor* StartStopMonitoringHelper(ActionType action_type,
                                              int render_process_id,
                                              int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // It's important that this code uses only the process id for lookup as there
  // may not be a RenderFrameHost or WebContents attached to the RenderProcess
  // at time of call; e.g., in the event of a crash.
  RenderProcessHost* const render_process_host =
      RenderProcessHost::FromID(render_process_id);
  if (!render_process_host)
    return nullptr;

  WebContentsImpl* const web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(
          RenderFrameHost::FromID(render_process_id, render_frame_id)));
  return web_contents ? web_contents->audio_stream_monitor() : nullptr;
}

}  // namespace

bool AudioStreamMonitor::StreamID::operator<(const StreamID& other) const {
  return std::tie(render_process_id, render_frame_id, stream_id) <
         std::tie(other.render_process_id, other.render_frame_id,
                  other.stream_id);
}

bool AudioStreamMonitor::StreamID::operator==(const StreamID& other) const {
  return std::tie(render_process_id, render_frame_id, stream_id) ==
         std::tie(other.render_process_id, other.render_frame_id,
                  other.stream_id);
}

AudioStreamMonitor::AudioStreamMonitor(WebContents* contents)
    : web_contents_(contents),
      clock_(&default_tick_clock_),
      was_recently_audible_(false),
      is_audible_(false) {
  DCHECK(web_contents_);
}

AudioStreamMonitor::~AudioStreamMonitor() {}

bool AudioStreamMonitor::WasRecentlyAudible() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return was_recently_audible_;
}

bool AudioStreamMonitor::IsCurrentlyAudible() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return is_audible_;
}

void AudioStreamMonitor::RenderProcessGone(int render_process_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Note: It's possible for the RenderProcessHost and WebContents (and thus
  // this class) to survive the death of the render process and subsequently be
  // reused. During this period StartStopMonitoringHelper() will be unable to
  // lookup the WebContents using the now-dead |render_frame_id|. We must thus
  // have this secondary mechanism for clearing stale callbacks.
  for (auto it = poll_callbacks_.begin(); it != poll_callbacks_.end();) {
    if (it->first.render_process_id == render_process_id) {
      it = poll_callbacks_.erase(it);
      OnStreamRemoved();
    } else {
      ++it;
    }
  }

  if (poll_callbacks_.empty())
    poll_timer_.Stop();
}

// static
void AudioStreamMonitor::StartMonitoringStream(
    int render_process_id,
    int render_frame_id,
    int stream_id,
    const ReadPowerAndClipCallback& read_power_callback) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&StartMonitoringHelper, render_process_id, render_frame_id,
                     stream_id, read_power_callback));
}

// static
void AudioStreamMonitor::StopMonitoringStream(int render_process_id,
                                              int render_frame_id,
                                              int stream_id) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&StopMonitoringHelper, render_process_id, render_frame_id,
                     stream_id));
}

// static
void AudioStreamMonitor::StartMonitoringHelper(
    int render_process_id,
    int render_frame_id,
    int stream_id,
    const ReadPowerAndClipCallback& read_power_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (AudioStreamMonitor* monitor = StartStopMonitoringHelper(
          ActionType::STARTING, render_process_id, render_frame_id)) {
    monitor->StartMonitoringStreamOnUIThread(render_process_id, render_frame_id,
                                             stream_id, read_power_callback);
  }
}

// static
void AudioStreamMonitor::StopMonitoringHelper(int render_process_id,
                                              int render_frame_id,
                                              int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (AudioStreamMonitor* monitor = StartStopMonitoringHelper(
          ActionType::STOPPING, render_process_id, render_frame_id)) {
    monitor->StopMonitoringStreamOnUIThread(render_process_id, render_frame_id,
                                            stream_id);
  }
}

void AudioStreamMonitor::StartMonitoringStreamOnUIThread(
    int render_process_id,
    int render_frame_id,
    int stream_id,
    const ReadPowerAndClipCallback& read_power_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!read_power_callback.is_null());

  const StreamID qualified_id = {render_process_id, render_frame_id, stream_id};
  DCHECK(poll_callbacks_.find(qualified_id) == poll_callbacks_.end());

  poll_callbacks_[qualified_id] = read_power_callback;

  // Sends audible signal to RenderFrameHost when there is no power level
  // monitoring, otherwise sends the signal when the stream becomes audible.
  if (!power_level_monitoring_available()) {
    if (auto* render_frame_host = static_cast<RenderFrameHostImpl*>(
            RenderFrameHost::FromID(render_process_id, render_frame_id))) {
      render_frame_host->OnAudibleStateChanged(true);
    }
  }

  OnStreamAdded();
}

void AudioStreamMonitor::StopMonitoringStreamOnUIThread(int render_process_id,
                                                        int render_frame_id,
                                                        int stream_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // In the event of render process death, these may have already been cleared.
  auto it = poll_callbacks_.find(
      StreamID{render_process_id, render_frame_id, stream_id});
  if (it == poll_callbacks_.end())
    return;

  poll_callbacks_.erase(it);

  // Sends non-audible signal to RenderFrameHost when there is no power level
  // monitoring, otherwise sends the signal when the stream becomes non-audible.
  if (!power_level_monitoring_available()) {
    if (auto* render_frame_host = static_cast<RenderFrameHostImpl*>(
            RenderFrameHost::FromID(render_process_id, render_frame_id))) {
      render_frame_host->OnAudibleStateChanged(false);
    }
  }

  OnStreamRemoved();
}

void AudioStreamMonitor::Poll() {
  bool was_audible = is_audible_;
  is_audible_ = false;

  // Record whether or not a RenderFrameHost is audible.
  base::flat_map<RenderFrameHostImpl*, bool> audible_frame_map;
  audible_frame_map.reserve(poll_callbacks_.size());
  for (auto& kv : poll_callbacks_) {
    // TODO(miu): A new UI for delivering specific power level and clipping
    // information is still in the works.  For now, we throw away all
    // information except for "is it audible?"
    const float power_dbfs = kv.second.Run().first;
    const float kSilenceThresholdDBFS = -72.24719896f;

    const bool is_stream_audible = power_dbfs >= kSilenceThresholdDBFS;
    if (!is_audible_ && is_stream_audible) {
      last_blurt_time_ = clock_->NowTicks();
      is_audible_ = true;
      MaybeToggle();
    }

    // Record whether or not the RenderFrame is audible. A RenderFrame is
    // audible when it has at least one audio stream that is audible.
    auto* render_frame_host_impl =
        static_cast<RenderFrameHostImpl*>(RenderFrameHost::FromID(
            kv.first.render_process_id, kv.first.render_frame_id));
    // This may be nullptr in tests.
    if (!render_frame_host_impl)
      continue;
    audible_frame_map[render_frame_host_impl] |= is_stream_audible;
  }

  // Update RenderFrameHost audible state only when state changed.
  for (auto& kv : audible_frame_map) {
    auto* render_frame_host_impl = kv.first;
    bool is_frame_audible = kv.second;
    if (is_frame_audible != render_frame_host_impl->is_audible())
      render_frame_host_impl->OnAudibleStateChanged(is_frame_audible);
  }

  if (is_audible_ != was_audible)
    web_contents_->OnAudioStateChanged(is_audible_);
}

void AudioStreamMonitor::MaybeToggle() {
  const bool indicator_was_on = was_recently_audible_;
  const base::TimeTicks off_time =
      last_blurt_time_ + base::TimeDelta::FromMilliseconds(kHoldOnMilliseconds);
  const base::TimeTicks now = clock_->NowTicks();
  const bool should_indicator_be_on = now < off_time;

  if (should_indicator_be_on != indicator_was_on) {
    was_recently_audible_ = should_indicator_be_on;
    web_contents_->NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
  }

  if (!should_indicator_be_on) {
    off_timer_.Stop();
  } else if (!off_timer_.IsRunning()) {
    off_timer_.Start(
        FROM_HERE,
        off_time - now,
        base::Bind(&AudioStreamMonitor::MaybeToggle, base::Unretained(this)));
  }
}

void AudioStreamMonitor::OnStreamAdded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (poll_callbacks_.size() != 1u)
    return;

  if (!power_level_monitoring_available()) {
    is_audible_ = true;
    web_contents_->OnAudioStateChanged(true);
    MaybeToggle();
  } else if (!poll_timer_.IsRunning()) {
    poll_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(1) /
                       static_cast<int>(kPowerMeasurementsPerSecond),
        base::Bind(&AudioStreamMonitor::Poll, base::Unretained(this)));
  }
}

void AudioStreamMonitor::OnStreamRemoved() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!poll_callbacks_.empty())
    return;

  if (!power_level_monitoring_available()) {
    is_audible_ = false;
    web_contents_->OnAudioStateChanged(false);
    MaybeToggle();
  } else {
    poll_timer_.Stop();
  }
}

}  // namespace content
