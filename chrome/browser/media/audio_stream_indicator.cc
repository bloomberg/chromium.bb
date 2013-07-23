// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/audio_stream_indicator.h"

#include <limits>

#include "base/bind.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using content::WebContents;

AudioStreamIndicator::AudioStreamIndicator() {}
AudioStreamIndicator::~AudioStreamIndicator() {}

void AudioStreamIndicator::UpdateWebContentsStatus(
    int render_process_id, int render_view_id, int stream_id,
    bool is_playing, float power_dbfs, bool clipped) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&AudioStreamIndicator::UpdateWebContentsStatusOnUIThread, this,
                 render_process_id, render_view_id, stream_id,
                 is_playing, power_dbfs, clipped));
}

bool AudioStreamIndicator::IsPlayingAudio(const WebContents* contents) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(miu): In order to prevent breaking existing uses of this method, the
  // old semantics of "playing AND not silent" have been retained here.  Once
  // the tab audio indicator UI switches over to using the new
  // GetAudioSignalPower(), this method should really be just "playing."
  float level;
  bool ignored;
  CurrentAudibleLevel(contents, &level, &ignored);
  return level > 0.0f;
}

void AudioStreamIndicator::CurrentAudibleLevel(
    const content::WebContents* contents, float* level, bool* clipped) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  float max_power_dbfs = -std::numeric_limits<float>::infinity();
  bool has_clipped = false;

  // Since a RenderView can have more than one stream playing back, return the
  // maximum of the last-reported power levels.  For more information about how
  // the power level is measured, see media/audio/audio_power_monitor.h.
  const RenderViewId id(contents->GetRenderProcessHost()->GetID(),
                        contents->GetRenderViewHost()->GetRoutingID());
  RenderViewStreamMap::const_iterator view_it = audio_streams_.find(id);
  if (view_it != audio_streams_.end()) {
    const StreamPowerLevels& stream_levels = view_it->second;
    for (StreamPowerLevels::const_iterator stream_it = stream_levels.begin();
         stream_it != stream_levels.end(); ++stream_it) {
      if (stream_it->power_dbfs > max_power_dbfs)
        max_power_dbfs = stream_it->power_dbfs;
      has_clipped |= stream_it->clipped;
    }
  }

  // Map the power into an "audible level" in the range [0.0,1.0].  dBFS values
  // are in the range -inf (minimum power) to 0.0 (maximum power).
  static const float kSilenceThresholdDBFS = -72.24719896f;
  if (max_power_dbfs < kSilenceThresholdDBFS)
    *level = 0.0f;
  else if (max_power_dbfs > 0.0f)
    *level = 1.0f;
  else
    *level = 1.0f - max_power_dbfs / kSilenceThresholdDBFS;
  *clipped = has_clipped;
}

void AudioStreamIndicator::UpdateWebContentsStatusOnUIThread(
    int render_process_id, int render_view_id, int stream_id,
    bool is_playing, float power_dbfs, bool clipped) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const RenderViewId id(render_process_id, render_view_id);
  if (is_playing) {
    // Find the StreamPowerLevel instance associated with |stream_id|, or
    // auto-create a new one.
    StreamPowerLevels& stream_levels = audio_streams_[id];
    StreamPowerLevels::iterator stream_it;
    for (stream_it = stream_levels.begin(); stream_it != stream_levels.end();
         ++stream_it) {
      if (stream_it->stream_id == stream_id)
        break;
    }
    if (stream_it == stream_levels.end()) {
      stream_it = stream_levels.insert(stream_levels.end(), StreamPowerLevel());
      stream_it->stream_id = stream_id;
    }

    // Update power and clip values.
    stream_it->power_dbfs = power_dbfs;
    stream_it->clipped = clipped;
  } else {
    // Find and erase the StreamPowerLevel instance associated with |stream_id|.
    RenderViewStreamMap::iterator view_it = audio_streams_.find(id);
    if (view_it != audio_streams_.end()) {
      StreamPowerLevels& stream_levels = view_it->second;
      for (StreamPowerLevels::iterator stream_it = stream_levels.begin();
           stream_it != stream_levels.end(); ++stream_it) {
        if (stream_it->stream_id == stream_id) {
          stream_levels.erase(stream_it);
          if (stream_levels.empty())
            audio_streams_.erase(view_it);
          break;
        }
      }
    }
  }

  // Trigger UI update.
  WebContents* web_contents = tab_util::GetWebContentsByID(render_process_id,
                                                           render_view_id);
  if (web_contents)
    web_contents->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
}
