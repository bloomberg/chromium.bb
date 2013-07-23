// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_AUDIO_STREAM_INDICATOR_H_
#define CHROME_BROWSER_MEDIA_AUDIO_STREAM_INDICATOR_H_

#include <map>
#include <vector>

#include "base/memory/ref_counted.h"

namespace content {
class WebContents;
}

class AudioStreamIndicator
    : public base::RefCountedThreadSafe<AudioStreamIndicator> {
 public:
  AudioStreamIndicator();

  // This method should be called on the IO thread.
  void UpdateWebContentsStatus(int render_process_id,
                               int render_view_id,
                               int stream_id,
                               bool is_playing,
                               float power_dbfs,
                               bool clipped);

  // This method should be called on the UI thread.
  bool IsPlayingAudio(const content::WebContents* contents);

  // Returns the audible |level| in the range [0.0,1.0], where 0.0 means the
  // audio signal is imperceivably silent and 1.0 means it is at maximum
  // volume.  |signal_has_clipped| is set to true if any part of the audio
  // signal has clipped since the last call to this method.
  //
  // This method should be called on the UI thread.
  void CurrentAudibleLevel(const content::WebContents* contents,
                           float* level, bool* signal_has_clipped);

 private:
  // <render process ID, render view ID>
  // Note: Using std::pair<> to reduce binary-size bloat.
  typedef std::pair<int, int> RenderViewId;
  struct StreamPowerLevel {
    int stream_id;
    float power_dbfs;
    bool clipped;
  };
  typedef std::vector<StreamPowerLevel> StreamPowerLevels;
  // Container for the power levels of streams playing from each render view.
  typedef std::map<RenderViewId, StreamPowerLevels> RenderViewStreamMap;

  friend class base::RefCountedThreadSafe<AudioStreamIndicator>;
  virtual ~AudioStreamIndicator();

  void UpdateWebContentsStatusOnUIThread(int render_process_id,
                                         int render_view_id,
                                         int stream_id,
                                         bool is_playing,
                                         float power_dbfs,
                                         bool clipped);

  RenderViewStreamMap audio_streams_;
};

#endif  // CHROME_BROWSER_MEDIA_AUDIO_STREAM_INDICATOR_H_
