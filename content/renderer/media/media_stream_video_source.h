// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_SOURCE_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_source.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace media {
class VideoFrame;
}

namespace content {

class MediaStreamDependencyFactory;

// MediaStreamVideoSource is an interface used for sending video frames to a
// MediaStreamVideoTrack.
// http://dev.w3.org/2011/webrtc/editor/getusermedia.html
// All methods calls will be done from the main render thread.
class CONTENT_EXPORT MediaStreamVideoSource
    : public MediaStreamSource,
      NON_EXPORTED_BASE(public webrtc::ObserverInterface),
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  explicit MediaStreamVideoSource(
      MediaStreamDependencyFactory* factory);


  // Puts |track| in the registered tracks list.
  virtual void AddTrack(const blink::WebMediaStreamTrack& track,
                        const blink::WebMediaConstraints& constraints,
                        const ConstraintsCallback& callback) OVERRIDE;
  virtual void RemoveTrack(const blink::WebMediaStreamTrack& track) OVERRIDE;

  // TODO(ronghuawu): Remove webrtc::VideoSourceInterface from the public
  // interface of this class.
  webrtc::VideoSourceInterface* GetAdapter() {
    return adapter_;
  }

 protected:
  virtual void DoStopSource() OVERRIDE {}

  // Called when the first track is added to this source.
  // It currently creates a webrtc::VideoSourceInterface.
  // If a derived class overrides this method, it must call SetAdapter.
  virtual void InitAdapter(const blink::WebMediaConstraints& constraints);

  // Set the webrtc::VideoSourceInterface adapter used by this class.
  // It must be called by a derived class that overrides the InitAdapter method.
  void SetAdapter(webrtc::VideoSourceInterface* adapter) {
    DCHECK(!adapter_);
    adapter_ = adapter;
  }

  MediaStreamDependencyFactory* factory() { return factory_; }

  // Sets ready state and notifies the ready state to all registered tracks.
  virtual void SetReadyState(blink::WebMediaStreamSource::ReadyState state);

  // Delivers |frame| to registered tracks according to their constraints.
  // Note: current implementation assumes |frame| be contiguous layout of image
  // planes and I420.
  virtual void DeliverVideoFrame(const scoped_refptr<media::VideoFrame>& frame);

  // Implements webrtc::Observer.
  virtual void OnChanged() OVERRIDE;

  virtual ~MediaStreamVideoSource();

 private:
  // Checks if the underlying source state has changed from an initializing
  // state to a final state and in that case trigger all callbacks in
  // |constraints_callbacks_|.
  void TriggerConstraintsCallbackOnStateChange();

  bool initializing_;
  MediaStreamDependencyFactory* factory_;
  scoped_refptr<webrtc::VideoSourceInterface> adapter_;
  int width_;
  int height_;
  base::TimeDelta first_frame_timestamp_;

  blink::WebMediaConstraints current_constraints_;
  std::vector<ConstraintsCallback> constraints_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_SOURCE_H_
