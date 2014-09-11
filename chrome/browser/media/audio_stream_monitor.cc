// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/audio_stream_monitor.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/web_contents.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(AudioStreamMonitor);

AudioStreamMonitor::AudioStreamMonitor(content::WebContents* contents)
    : web_contents_(contents),
      clock_(&default_tick_clock_),
      was_recently_audible_(false) {
  DCHECK(web_contents_);
}

AudioStreamMonitor::~AudioStreamMonitor() {}

bool AudioStreamMonitor::WasRecentlyAudible() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return was_recently_audible_;
}

void AudioStreamMonitor::StartMonitoringStream(
    int stream_id,
    const ReadPowerAndClipCallback& read_power_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!read_power_callback.is_null());
  poll_callbacks_[stream_id] = read_power_callback;
  if (!poll_timer_.IsRunning()) {
    poll_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(1) / kPowerMeasurementsPerSecond,
        base::Bind(&AudioStreamMonitor::Poll, base::Unretained(this)));
  }
}

void AudioStreamMonitor::StopMonitoringStream(int stream_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  poll_callbacks_.erase(stream_id);
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
  const bool indicator_was_on = was_recently_audible_;
  const base::TimeTicks off_time =
      last_blurt_time_ + base::TimeDelta::FromMilliseconds(kHoldOnMilliseconds);
  const base::TimeTicks now = clock_->NowTicks();
  const bool should_indicator_be_on = now < off_time;

  if (should_indicator_be_on != indicator_was_on) {
    was_recently_audible_ = should_indicator_be_on;
    web_contents_->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
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
