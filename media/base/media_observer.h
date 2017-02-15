// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_OBSERVER_H_
#define MEDIA_BASE_MEDIA_OBSERVER_H_

#include "media/base/cdm_context.h"
#include "media/base/pipeline_metadata.h"
#include "url/gurl.h"

namespace media {

class MEDIA_EXPORT MediaObserverClient {
 public:
  virtual ~MediaObserverClient() {}

  // Requests to restart the media pipeline and create a new renderer as soon as
  // possible. |disable_pipeline_auto_suspend| indicates whether to disable
  // any optimizations that might suspend the media pipeline.
  virtual void SwitchRenderer(bool disable_pipeline_auto_suspend) = 0;

  // Requests to activate monitoring changes on viewport intersection.
  virtual void ActivateViewportIntersectionMonitoring(bool activate) = 0;
};

// This class is an observer of media player events.
class MEDIA_EXPORT MediaObserver {
 public:
  MediaObserver();
  virtual ~MediaObserver();

  // Called when the media element entered/exited fullscreen.
  virtual void OnEnteredFullscreen() = 0;
  virtual void OnExitedFullscreen() = 0;

  // Called when the media element starts/stops being the dominant visible
  // content.
  virtual void OnBecameDominantVisibleContent(bool is_dominant) {}

  // Called when CDM is attached to the media element. The |cdm_context| is
  // only guaranteed to be valid in this call.
  virtual void OnSetCdm(CdmContext* cdm_context) = 0;

  // Called after demuxer is initialized.
  virtual void OnMetadataChanged(const PipelineMetadata& metadata) = 0;

  // Called to indicate whether the site requests that remote playback be
  // disabled. The "disabled" naming corresponds with the
  // "disableRemotePlayback" media element attribute, as described in the
  // Remote Playback API spec: https://w3c.github.io/remote-playback
  virtual void OnRemotePlaybackDisabled(bool disabled) = 0;

  // Called when the media is playing/paused.
  virtual void OnPlaying() = 0;
  virtual void OnPaused() = 0;

  // Called when a poster image URL is set, which happens when media is loaded
  // or the poster attribute is changed.
  virtual void OnSetPoster(const GURL& poster) = 0;

  // Set the MediaObserverClient.
  virtual void SetClient(MediaObserverClient* client) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_OBSERVER_H_
