// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_track.h"

#include "base/bind.h"
#include "content/renderer/media/video_frame_deliverer.h"
#include "media/base/bind_to_current_loop.h"

namespace content {

// Helper class used for delivering video frames to MediaStreamSinks on the
// IO-thread.
// Frames are delivered to an instance of this class from a
// MediaStreamVideoSource on the IO-thread to the method DeliverFrameOnIO.
// Frames are only delivered to the sinks if the track is enabled.
class MediaStreamVideoTrack::FrameDeliverer : public VideoFrameDeliverer {
 public:
  FrameDeliverer(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop_proxy,
      bool enabled)
      : VideoFrameDeliverer(io_message_loop_proxy),
        enabled_(enabled) {
  }

  // Add |sink| to receive state changes on the main render thread.
  // Video frames will be delivered to |callback| on the IO thread.
  void AddSink(MediaStreamVideoSink* sink,
               const VideoCaptureDeliverFrameCB& callback) {
    DCHECK(thread_checker().CalledOnValidThread());
    DCHECK(std::find(sinks_.begin(), sinks_.end(), sink) == sinks_.end());
    sinks_.push_back(sink);
    AddCallback(sink, callback);
  }

  void RemoveSink(MediaStreamVideoSink* sink) {
    DCHECK(thread_checker().CalledOnValidThread());
    std::vector<MediaStreamVideoSink*>::iterator it =
        std::find(sinks_.begin(), sinks_.end(), sink);
    DCHECK(it != sinks_.end());
    sinks_.erase(it);
    RemoveCallback(sink);
  }

  void SetEnabled(bool enabled) {
    DCHECK(thread_checker().CalledOnValidThread());
    io_message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&MediaStreamVideoTrack::FrameDeliverer::SetEnabledOnIO,
                   this, enabled));
  }

  virtual void DeliverFrameOnIO(
      const scoped_refptr<media::VideoFrame>& frame,
      const media::VideoCaptureFormat& format) OVERRIDE {
    DCHECK(io_message_loop()->BelongsToCurrentThread());
    if (!enabled_)
      return;
    VideoFrameDeliverer::DeliverFrameOnIO(frame, format);
  }

  const std::vector<MediaStreamVideoSink*>& sinks() const { return sinks_; }

 protected:
  virtual ~FrameDeliverer() {
    DCHECK(sinks_.empty());
  }

  void SetEnabledOnIO(bool enabled) {
    DCHECK(io_message_loop()->BelongsToCurrentThread());
    enabled_ = enabled;
  }

 private:
  // The below members are used on the main render thread.
  std::vector<MediaStreamVideoSink*> sinks_;

  // The below parameters are used on the IO-thread.
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(FrameDeliverer);
};

// static
blink::WebMediaStreamTrack MediaStreamVideoTrack::CreateVideoTrack(
    MediaStreamVideoSource* source,
    const blink::WebMediaConstraints& constraints,
    const MediaStreamVideoSource::ConstraintsCallback& callback,
    bool enabled) {
  blink::WebMediaStreamTrack track;
  track.initialize(source->owner());
  track.setExtraData(new MediaStreamVideoTrack(source,
                                               constraints,
                                               callback,
                                               enabled));
  return track;
}

// static
MediaStreamVideoTrack* MediaStreamVideoTrack::GetVideoTrack(
     const blink::WebMediaStreamTrack& track) {
  return static_cast<MediaStreamVideoTrack*>(track.extraData());
}

MediaStreamVideoTrack::MediaStreamVideoTrack(
    MediaStreamVideoSource* source,
    const blink::WebMediaConstraints& constraints,
    const MediaStreamVideoSource::ConstraintsCallback& callback,
    bool enabled)
    : MediaStreamTrack(NULL, true),
      frame_deliverer_(
          new MediaStreamVideoTrack::FrameDeliverer(source->io_message_loop(),
                                                    enabled)),
      constraints_(constraints),
      source_(source) {
  DCHECK(!constraints.isNull());
  source->AddTrack(this,
                   base::Bind(
                       &MediaStreamVideoTrack::FrameDeliverer::DeliverFrameOnIO,
                       frame_deliverer_),
                   constraints, callback);
}

MediaStreamVideoTrack::~MediaStreamVideoTrack() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Stop();
  DVLOG(3) << "~MediaStreamVideoTrack()";
}

void MediaStreamVideoTrack::AddSink(
    MediaStreamVideoSink* sink, const VideoCaptureDeliverFrameCB& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  frame_deliverer_->AddSink(sink, callback);
}

void MediaStreamVideoTrack::RemoveSink(MediaStreamVideoSink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  frame_deliverer_->RemoveSink(sink);
}

void MediaStreamVideoTrack::SetEnabled(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  MediaStreamTrack::SetEnabled(enabled);

  frame_deliverer_->SetEnabled(enabled);
  const std::vector<MediaStreamVideoSink*>& sinks = frame_deliverer_->sinks();
  for (std::vector<MediaStreamVideoSink*>::const_iterator it = sinks.begin();
       it != sinks.end(); ++it) {
    (*it)->OnEnabledChanged(enabled);
  }
}

void MediaStreamVideoTrack::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (source_) {
    source_->RemoveTrack(this);
    source_ = NULL;
  }
  OnReadyStateChanged(blink::WebMediaStreamSource::ReadyStateEnded);
}

void MediaStreamVideoTrack::OnReadyStateChanged(
    blink::WebMediaStreamSource::ReadyState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const std::vector<MediaStreamVideoSink*>& sinks = frame_deliverer_->sinks();
  for (std::vector<MediaStreamVideoSink*>::const_iterator it = sinks.begin();
       it != sinks.end(); ++it) {
    (*it)->OnReadyStateChanged(state);
  }
}

}  // namespace content
