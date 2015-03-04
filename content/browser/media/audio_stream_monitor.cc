// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_stream_monitor.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/render_frame_host.h"

namespace content {

namespace {

AudioStreamMonitor* AudioStreamMonitorFromRenderFrame(int render_process_id,
                                                      int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebContentsImpl* const web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(
          RenderFrameHost::FromID(render_process_id, render_frame_id)));

  if (!web_contents)
    return nullptr;

  AudioStateProvider* audio_provider = web_contents->audio_state_provider();
  return audio_provider ? audio_provider->audio_stream_monitor() : nullptr;
}

}  // namespace

AudioStreamMonitor::AudioStreamMonitor(WebContents* contents)
    : AudioStateProvider(contents),
      clock_(&default_tick_clock_)
{
}

AudioStreamMonitor::~AudioStreamMonitor() {}

bool AudioStreamMonitor::IsAudioStateAvailable() const {
  return media::AudioOutputController::will_monitor_audio_levels();
}

// This provider is the monitor.
AudioStreamMonitor* AudioStreamMonitor::audio_stream_monitor() {
  return this;
}

bool AudioStreamMonitor::WasRecentlyAudible() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return AudioStateProvider::WasRecentlyAudible();
}

// static
void AudioStreamMonitor::StartMonitoringStream(
    int render_process_id,
    int render_frame_id,
    int stream_id,
    const ReadPowerAndClipCallback& read_power_callback) {
  if (!media::AudioOutputController::will_monitor_audio_levels())
    return;
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&StartMonitoringHelper,
                                     render_process_id,
                                     render_frame_id,
                                     stream_id,
                                     read_power_callback));
}

// static
void AudioStreamMonitor::StopMonitoringStream(int render_process_id,
                                              int render_frame_id,
                                              int stream_id) {
  if (!media::AudioOutputController::will_monitor_audio_levels())
    return;
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&StopMonitoringHelper,
                                     render_process_id,
                                     render_frame_id,
                                     stream_id));
}

// static
void AudioStreamMonitor::StartMonitoringHelper(
    int render_process_id,
    int render_frame_id,
    int stream_id,
    const ReadPowerAndClipCallback& read_power_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AudioStreamMonitor* const monitor =
      AudioStreamMonitorFromRenderFrame(render_process_id, render_frame_id);
  if (monitor) {
    monitor->StartMonitoringStreamOnUIThread(
        render_process_id, stream_id, read_power_callback);
  }
}

// static
void AudioStreamMonitor::StopMonitoringHelper(int render_process_id,
                                              int render_frame_id,
                                              int stream_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AudioStreamMonitor* const monitor =
      AudioStreamMonitorFromRenderFrame(render_process_id, render_frame_id);
  if (monitor)
    monitor->StopMonitoringStreamOnUIThread(render_process_id, stream_id);
}

void AudioStreamMonitor::StartMonitoringStreamOnUIThread(
    int render_process_id,
    int stream_id,
    const ReadPowerAndClipCallback& read_power_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!read_power_callback.is_null());
  poll_callbacks_[StreamID(render_process_id, stream_id)] = read_power_callback;
  if (!poll_timer_.IsRunning()) {
    poll_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(1) / kPowerMeasurementsPerSecond,
        base::Bind(&AudioStreamMonitor::Poll, base::Unretained(this)));
  }
}

void AudioStreamMonitor::StopMonitoringStreamOnUIThread(int render_process_id,
                                                        int stream_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  poll_callbacks_.erase(StreamID(render_process_id, stream_id));
  if (poll_callbacks_.empty())
    poll_timer_.Stop();
}

void AudioStreamMonitor::Poll() {
  for (StreamPollCallbackMap::const_iterator it = poll_callbacks_.begin();
       it != poll_callbacks_.end();
       ++it) {
    // TODO(miu): A new UI for delivering specific power level and clipping
    // information is still in the works.  For now, we throw away all
    // information except for "is it audible?"
    const float power_dbfs = it->second.Run().first;
    const float kSilenceThresholdDBFS = -72.24719896f;
    if (power_dbfs >= kSilenceThresholdDBFS) {
      last_blurt_time_ = clock_->NowTicks();
      MaybeToggle();
      break;  // No need to poll remaining streams.
    }
  }
}

void AudioStreamMonitor::MaybeToggle() {
  const base::TimeTicks off_time =
      last_blurt_time_ + base::TimeDelta::FromMilliseconds(kHoldOnMilliseconds);
  const base::TimeTicks now = clock_->NowTicks();
  const bool should_indicator_be_on = now < off_time;

  Notify(should_indicator_be_on);

  if (!should_indicator_be_on) {
    off_timer_.Stop();
  } else if (!off_timer_.IsRunning()) {
    off_timer_.Start(
        FROM_HERE,
        off_time - now,
        base::Bind(&AudioStreamMonitor::MaybeToggle, base::Unretained(this)));
  }
}

}  // namespace content
