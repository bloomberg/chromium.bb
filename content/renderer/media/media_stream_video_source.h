// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_SOURCE_H_

#include <vector>

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_source.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_pool.h"
#include "media/video/capture/video_capture_types.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace media {
class VideoFrame;
}

namespace content {

class MediaStreamDependencyFactory;
class MediaStreamVideoTrack;
class WebRtcVideoCapturerAdapter;

// MediaStreamVideoSource is an interface used for sending video frames to a
// MediaStreamVideoTrack.
// http://dev.w3.org/2011/webrtc/editor/getusermedia.html
// The purpose of this base class is to be able to implement different
// MediaStreaVideoSources such as local video capture, video sources received
// on a PeerConnection or a source created in NaCl.
// All methods calls will be done from the main render thread.
//
// When  the first track is added to the source by calling AddTrack
// the MediaStreamVideoSource implementation calls GetCurrentSupportedFormats.
// the source implementation must call OnSupportedFormats.
// MediaStreamVideoSource then match the constraints provided in AddTrack with
// the formats and call StartSourceImpl. The source implementation must call
// OnStartDone when the underlying source has been started or failed to
// start.
class CONTENT_EXPORT MediaStreamVideoSource
    : public MediaStreamSource,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  explicit MediaStreamVideoSource(MediaStreamDependencyFactory* factory);
  virtual ~MediaStreamVideoSource();

  // Returns the MediaStreamVideoSource object owned by |source|.
  static MediaStreamVideoSource* GetVideoSource(
      const blink::WebMediaStreamSource& source);

  // Puts |track| in the registered tracks list.
  void AddTrack(MediaStreamVideoTrack* track,
                const blink::WebMediaConstraints& constraints,
                const ConstraintsCallback& callback);
  void RemoveTrack(MediaStreamVideoTrack* track);

  // TODO(ronghuawu): Remove webrtc::VideoSourceInterface from the public
  // interface of this class.
  // This creates a VideoSourceInterface implementation if it does not already
  // exist.
  virtual webrtc::VideoSourceInterface* GetAdapter();

  // Return true if |name| is a constraint supported by MediaStreamVideoSource.
  static bool IsConstraintSupported(const std::string& name);

  // Constraint keys used by a video source.
  // Specified by draft-alvestrand-constraints-resolution-00b
  static const char kMinAspectRatio[];  // minAspectRatio
  static const char kMaxAspectRatio[];  // maxAspectRatio
  static const char kMaxWidth[];  // maxWidth
  static const char kMinWidth[];  // minWidthOnCaptureFormats
  static const char kMaxHeight[];  // maxHeight
  static const char kMinHeight[];  // minHeight
  static const char kMaxFrameRate[];  // maxFrameRate
  static const char kMinFrameRate[];  // minFrameRate

  // Default resolution. If no constraints are specified and the delegate
  // support it, this is the resolution that will be used.
  static const int kDefaultWidth;
  static const int kDefaultHeight;
  static const int kDefaultFrameRate;

 protected:
  virtual void DoStopSource() OVERRIDE;

  MediaStreamDependencyFactory* factory() { return factory_; }

  // Sets ready state and notifies the ready state to all registered tracks.
  virtual void SetReadyState(blink::WebMediaStreamSource::ReadyState state);

  // Delivers |frame| to registered tracks according to their constraints.
  // Note: current implementation assumes |frame| be contiguous layout of image
  // planes and I420.
  virtual void DeliverVideoFrame(const scoped_refptr<media::VideoFrame>& frame);

  // An implementation must fetch the formats that can currently be used by
  // the source and call OnSupportedFormats when done.
  // |max_requested_height| and |max_requested_width| is the max height and
  // width set as a mandatory constraint if set when calling
  // MediaStreamVideoSource::AddTrack. If max height and max width is not set
  // |max_requested_height| and |max_requested_width| are 0.
  virtual void GetCurrentSupportedFormats(int max_requested_width,
                                          int max_requested_height) = 0;
  void OnSupportedFormats(const media::VideoCaptureFormats& formats);

  // An implementation must start capture frames using the resolution in
  // |params|. When the source has started or the source failed to start
  // OnStartDone must be called. An implementation must call
  // DeliverVideoFrame with the captured frames.
  // TODO(perkj): pass a VideoCaptureFormats instead of VideoCaptureParams for
  // subclasses to customize.
  virtual void StartSourceImpl(const media::VideoCaptureParams& params) = 0;
  void OnStartDone(bool success);

  // An implementation must immediately stop capture video frames and must not
  // call OnSupportedFormats after this method has been called. After this
  // method has been called, MediaStreamVideoSource may be deleted.
  virtual void StopSourceImpl() = 0;

  enum State {
    NEW,
    RETRIEVING_CAPABILITIES,
    STARTING,
    STARTED,
    ENDED
  };
  State state() const { return state_; }

 private:
  // Creates a webrtc::VideoSourceInterface used by libjingle.
  void InitAdapter();

  // Finds the first constraints in |requested_constraints_| that can be
  // fulfilled. |best_format| is set to the video resolution that can be
  // fulfilled. |frame_output_size| is the requested frame size after cropping.
  // |resulting_constraints| is set to the found constraints in
  // |requested_constraints_|.
  bool FindBestFormatWithConstraints(
      const media::VideoCaptureFormats& formats,
      media::VideoCaptureFormat* best_format,
      gfx::Size* frame_output_size,
      blink::WebMediaConstraints* resulting_constraints);

  // Trigger all cached callbacks from AddTrack. AddTrack is successful
  // if the capture delegate has started and the constraints provided in
  // AddTrack match the format that was used to start the device.
  void FinalizeAddTrack();

  State state_;

  media::VideoCaptureFormat current_format_;
  blink::WebMediaConstraints current_constraints_;
  gfx::Size frame_output_size_;

  struct RequestedConstraints {
    RequestedConstraints(const blink::WebMediaConstraints& constraints,
                         const ConstraintsCallback& callback);
    ~RequestedConstraints();

    blink::WebMediaConstraints constraints;
    ConstraintsCallback callback;
  };
  std::vector<RequestedConstraints> requested_constraints_;

  media::VideoCaptureFormats supported_formats_;

  // Tracks that currently are receiving video frames.
  std::vector<MediaStreamVideoTrack*> tracks_;

  // TODO(perkj): The below classes use webrtc/libjingle types. The goal is to
  // get rid of them as far as possible.
  MediaStreamDependencyFactory* factory_;
  scoped_refptr<webrtc::VideoSourceInterface> adapter_;
  WebRtcVideoCapturerAdapter* capture_adapter_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_SOURCE_H_
