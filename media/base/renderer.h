// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_RENDERER_H_
#define MEDIA_BASE_RENDERER_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "media/base/buffering_state.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"

namespace media {

class MediaKeys;

class MEDIA_EXPORT Renderer {
 public:
  typedef base::Callback<base::TimeDelta()> TimeDeltaCB;

  Renderer();

  // Stops rendering and fires any pending callbacks.
  virtual ~Renderer();

  // Initializes the Renderer, executing |init_cb| upon completion.
  // If initialization failed, fires |error_cb| before |init_cb|.
  //
  // Permanent callbacks:
  // - |statistics_cb|: Executed periodically with rendering statistics.
  // - |time_cb|: Executed whenever time has advanced through rendering.
  // - |ended_cb|: Executed when rendering has reached the end of stream.
  // - |error_cb|: Executed if any error was encountered during rendering.
  virtual void Initialize(const base::Closure& init_cb,
                          const StatisticsCB& statistics_cb,
                          const base::Closure& ended_cb,
                          const PipelineStatusCB& error_cb,
                          const BufferingStateCB& buffering_state_cb) = 0;

  // The following functions must be called after Initialize().

  // Discards any buffered data, executing |flush_cb| when completed.
  virtual void Flush(const base::Closure& flush_cb) = 0;

  // Starts rendering from |time|.
  virtual void StartPlayingFrom(base::TimeDelta time) = 0;

  // Updates the current playback rate. The default playback rate should be 1.
  virtual void SetPlaybackRate(float playback_rate) = 0;

  // Sets the output volume. The default volume should be 1.
  virtual void SetVolume(float volume) = 0;

  // Returns the current media time.
  virtual base::TimeDelta GetMediaTime() = 0;

  // Returns whether |this| renders audio.
  virtual bool HasAudio() = 0;

  // Returns whether |this| renders video.
  virtual bool HasVideo() = 0;

  // Associates the |cdm| with this Renderer.
  virtual void SetCdm(MediaKeys* cdm) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Renderer);
};

}  // namespace media

#endif  // MEDIA_BASE_RENDERER_H_
