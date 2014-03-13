// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_track_metrics_host.h"

#include "base/metrics/histogram.h"
#include "content/common/media/media_stream_track_metrics_host_messages.h"

// We use a histogram with a maximum bucket of 16 hours to infinity
// for track durations.
#define UMA_HISTOGRAM_TIMES_16H(name, sample)                        \
  UMA_HISTOGRAM_CUSTOM_TIMES(name, sample,                           \
                             base::TimeDelta::FromMilliseconds(100), \
                             base::TimeDelta::FromHours(16),         \
                             50);

namespace content {

MediaStreamTrackMetricsHost::MediaStreamTrackMetricsHost()
    : BrowserMessageFilter(MediaStreamTrackMetricsHostMsgStart) {
}

MediaStreamTrackMetricsHost::~MediaStreamTrackMetricsHost() {
  // Our render process has exited. We won't receive any more IPC
  // messages from it. Assume all tracks ended now.
  for (TrackMap::iterator it = tracks_.begin();
       it != tracks_.end();
       ++it) {
    IsAudioPlusTimestamp& audio_and_timestamp = it->second;
    ReportDuration(audio_and_timestamp.first, audio_and_timestamp.second);
  }
  tracks_.clear();
}

bool MediaStreamTrackMetricsHost::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP_EX(MediaStreamTrackMetricsHost,
                           message,
                           *message_was_ok)
    IPC_MESSAGE_HANDLER(MediaStreamTrackMetricsHost_AddTrack, OnAddTrack)
    IPC_MESSAGE_HANDLER(MediaStreamTrackMetricsHost_RemoveTrack, OnRemoveTrack)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void MediaStreamTrackMetricsHost::OnAddTrack(uint64 id,
                                             bool is_audio,
                                             bool is_remote) {
  DCHECK(tracks_.find(id) == tracks_.end());
  DCHECK(is_remote);  // Always the case for now.

  tracks_[id] = IsAudioPlusTimestamp(is_audio, base::TimeTicks::Now());
}

void MediaStreamTrackMetricsHost::OnRemoveTrack(uint64 id) {
  DCHECK(tracks_.find(id) != tracks_.end());

  IsAudioPlusTimestamp& info = tracks_[id];
  ReportDuration(info.first, info.second);
  tracks_.erase(id);
}

void MediaStreamTrackMetricsHost::ReportDuration(bool is_audio,
                                                 base::TimeTicks start_time) {
  base::TimeDelta duration = base::TimeTicks::Now() - start_time;
  if (is_audio) {
    UMA_HISTOGRAM_TIMES_16H("WebRTC.ReceivedAudioTrackDuration", duration);
  } else {
    UMA_HISTOGRAM_TIMES_16H("WebRTC.ReceivedVideoTrackDuration", duration);
  }
}

}  // namespace content
