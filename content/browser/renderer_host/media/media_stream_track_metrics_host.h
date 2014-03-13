// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_TRACK_METRICS_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_TRACK_METRICS_HOST_H_

#include <map>
#include <string>

#include "base/time/time.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

// Responsible for logging metrics about audio and video track
// lifetimes. These are based on messages from the renderer that are
// sent when tracks are created and destroyed. Unfortunately we can't
// reliably log the lifetime metric in the renderer because that
// process may be destroyed at any time by the fast shutdown path (see
// RenderProcessHost::FastShutdownIfPossible).
//
// There is one instance of this class per render process.
//
// If the renderer process goes away without sending messages that
// tracks were removed, this class instead infers that the tracks were
// removed.
class MediaStreamTrackMetricsHost
    : public BrowserMessageFilter {
 public:
  explicit MediaStreamTrackMetricsHost();

 protected:
  virtual ~MediaStreamTrackMetricsHost();

  // BrowserMessageFilter override.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 private:
  void OnAddTrack(uint64 id, bool is_audio, bool is_remote);
  void OnRemoveTrack(uint64 id);

  void ReportDuration(bool is_audio, base::TimeTicks start_time);

  // Keys are unique (per renderer) track IDs, values are pairs: A
  // boolean indicating whether the track is an audio track, and a
  // timestamp from when the track was created.
  typedef std::pair<bool, base::TimeTicks> IsAudioPlusTimestamp;
  typedef std::map<uint64, IsAudioPlusTimestamp> TrackMap;
  TrackMap tracks_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_TRACK_METRICS_HOST_H_
