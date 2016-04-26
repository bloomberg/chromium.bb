// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PIPELINE_H_
#define MEDIA_BASE_PIPELINE_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/buffering_state.h"
#include "media/base/cdm_context.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "media/base/ranges.h"
#include "media/base/text_track.h"
#include "media/base/video_rotation.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class Demuxer;
class Renderer;
class VideoFrame;

// Metadata describing a pipeline once it has been initialized.
struct PipelineMetadata {
  PipelineMetadata()
      : has_audio(false), has_video(false), video_rotation(VIDEO_ROTATION_0) {}

  bool has_audio;
  bool has_video;
  gfx::Size natural_size;
  VideoRotation video_rotation;
  base::Time timeline_offset;
};

typedef base::Callback<void(PipelineMetadata)> PipelineMetadataCB;

class MEDIA_EXPORT Pipeline {
 public:
  // Used to paint VideoFrame.
  typedef base::Callback<void(const scoped_refptr<VideoFrame>&)> PaintCB;

  // Build a pipeline to using the given |demuxer| and |renderer| to construct
  // a filter chain, executing |seek_cb| when the initial seek has completed.
  //
  // The following permanent callbacks will be executed as follows up until
  // Stop() has completed:
  //   |ended_cb| will be executed whenever the media reaches the end.
  //   |error_cb| will be executed whenever an error occurs but hasn't been
  //              reported already through another callback.
  //   |metadata_cb| will be executed when the content duration, container video
  //                 size, start time, and whether the content has audio and/or
  //                 video in supported formats are known.
  //   |buffering_state_cb| will be executed whenever there are changes in the
  //                        overall buffering state of the pipeline.
  //   |duration_change_cb| optional callback that will be executed whenever the
  //                        presentation duration changes.
  //   |add_text_track_cb| will be executed whenever a text track is added.
  //   |waiting_for_decryption_key_cb| will be executed whenever the key needed
  //                                   to decrypt the stream is not available.
  // It is an error to call this method after the pipeline has already started.
  virtual void Start(Demuxer* demuxer,
                     std::unique_ptr<Renderer> renderer,
                     const base::Closure& ended_cb,
                     const PipelineStatusCB& error_cb,
                     const PipelineStatusCB& seek_cb,
                     const PipelineMetadataCB& metadata_cb,
                     const BufferingStateCB& buffering_state_cb,
                     const base::Closure& duration_change_cb,
                     const AddTextTrackCB& add_text_track_cb,
                     const base::Closure& waiting_for_decryption_key_cb) = 0;

  // Asynchronously stops the pipeline, executing |stop_cb| when the pipeline
  // teardown has completed.
  //
  // Stop() must complete before destroying the pipeline. It it permissible to
  // call Stop() at any point during the lifetime of the pipeline.
  //
  // It is safe to delete the pipeline during the execution of |stop_cb|.
  virtual void Stop(const base::Closure& stop_cb) = 0;

  // Attempt to seek to the position specified by time.  |seek_cb| will be
  // executed when the all filters in the pipeline have processed the seek.
  //
  // Clients are expected to call GetMediaTime() to check whether the seek
  // succeeded.
  //
  // It is an error to call this method if the pipeline has not started or
  // has been suspended.
  virtual void Seek(base::TimeDelta time, const PipelineStatusCB& seek_cb) = 0;

  // Suspends the pipeline, discarding the current renderer.
  //
  // While suspended, GetMediaTime() returns the presentation timestamp of the
  // last rendered frame.
  //
  // It is an error to call this method if the pipeline has not started or is
  // seeking.
  virtual void Suspend(const PipelineStatusCB& suspend_cb) = 0;

  // Resume the pipeline with a new renderer, and initialize it with a seek.
  //
  // It is an error to call this method if the pipeline has not finished
  // suspending.
  virtual void Resume(std::unique_ptr<Renderer> renderer,
                      base::TimeDelta timestamp,
                      const PipelineStatusCB& seek_cb) = 0;

  // Returns true if the pipeline has been started via Start().  If IsRunning()
  // returns true, it is expected that Stop() will be called before destroying
  // the pipeline.
  virtual bool IsRunning() const = 0;

  // Gets the current playback rate of the pipeline.  When the pipeline is
  // started, the playback rate will be 0.0.  A rate of 1.0 indicates
  // that the pipeline is rendering the media at the standard rate.  Valid
  // values for playback rate are >= 0.0.
  virtual double GetPlaybackRate() const = 0;

  // Attempt to adjust the playback rate. Setting a playback rate of 0.0 pauses
  // all rendering of the media.  A rate of 1.0 indicates a normal playback
  // rate.  Values for the playback rate must be greater than or equal to 0.0.
  //
  // TODO(scherkus): What about maximum rate?  Does HTML5 specify a max?
  virtual void SetPlaybackRate(double playback_rate) = 0;

  // Gets the current volume setting being used by the audio renderer.  When
  // the pipeline is started, this value will be 1.0f.  Valid values range
  // from 0.0f to 1.0f.
  virtual float GetVolume() const = 0;

  // Attempt to set the volume of the audio renderer.  Valid values for volume
  // range from 0.0f (muted) to 1.0f (full volume).  This value affects all
  // channels proportionately for multi-channel audio streams.
  virtual void SetVolume(float volume) = 0;

  // Returns the current media playback time, which progresses from 0 until
  // GetMediaDuration().
  virtual base::TimeDelta GetMediaTime() const = 0;

  // Get approximate time ranges of buffered media.
  virtual Ranges<base::TimeDelta> GetBufferedTimeRanges() const = 0;

  // Get the duration of the media in microseconds.  If the duration has not
  // been determined yet, then returns 0.
  virtual base::TimeDelta GetMediaDuration() const = 0;

  // Return true if loading progress has been made since the last time this
  // method was called.
  virtual bool DidLoadingProgress() = 0;

  // Gets the current pipeline statistics.
  virtual PipelineStatistics GetStatistics() const = 0;

  virtual void SetCdm(CdmContext* cdm_context,
                      const CdmAttachedCB& cdm_attached_cb) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_H_
