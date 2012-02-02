// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FilterHost describes an interface for individual filters to access and
// modify global playback information.  Every filter is given a filter host
// reference as part of initialization.
//
// This interface is intentionally verbose to cover the needs for the different
// types of filters (see media/base/filters.h for filter definitions).  Filters
// typically use parts of the interface that are relevant to their function.
// For example, an audio renderer filter typically calls SetTime as it feeds
// data to the audio hardware.  A video renderer filter typically calls GetTime
// to synchronize video with audio.  An audio and video decoder would typically
// have no need to call either SetTime or GetTime.

#ifndef MEDIA_BASE_FILTER_HOST_H_
#define MEDIA_BASE_FILTER_HOST_H_

#include "base/time.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "ui/gfx/size.h"

namespace media {

class MEDIA_EXPORT FilterHost {
 public:
  // Stops execution of the pipeline due to a fatal error.  Do not call this
  // method with PIPELINE_OK.
  virtual void SetError(PipelineStatus error) = 0;

  // Gets the current time.
  virtual base::TimeDelta GetTime() const = 0;

  // Gets the duration.
  virtual base::TimeDelta GetDuration() const = 0;

  // Sets the natural size of the video output in pixel units.
  virtual void SetNaturalVideoSize(const gfx::Size& size) = 0;

  // Notifies that this filter has ended, typically only called by filter graph
  // endpoints such as renderers.
  virtual void NotifyEnded() = 0;

  // Disable audio renderer by calling OnAudioRendererDisabled() on all
  // filters.
  virtual void DisableAudioRenderer() = 0;

 protected:
  virtual ~FilterHost() {}
};

}  // namespace media

#endif  // MEDIA_BASE_FILTER_HOST_H_
