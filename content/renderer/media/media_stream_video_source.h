// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_SOURCE_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "content/renderer/media/media_stream_source.h"
#include "content/renderer/media/secure_display_link_tracker.h"
#include "media/base/video_frame.h"
#include "media/capture/video_capture_types.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace content {

class MediaStreamVideoTrack;
class VideoTrackAdapter;
struct VideoTrackAdapterSettings;

// MediaStreamVideoSource is an interface used for sending video frames to a
// MediaStreamVideoTrack.
// http://dev.w3.org/2011/webrtc/editor/getusermedia.html
// The purpose of this base class is to be able to implement different
// MediaStreaVideoSources such as local video capture, video sources received
// on a PeerConnection or a source created in NaCl.
// All methods calls will be done from the main render thread.
class CONTENT_EXPORT MediaStreamVideoSource : public MediaStreamSource {
 public:
  enum {
    // Default resolution. If no constraints are specified and the delegate
    // support it, this is the resolution that will be used.
    kDefaultWidth = 640,
    kDefaultHeight = 480,

    kDefaultFrameRate = 30,
    kUnknownFrameRate = 0,
  };

  static constexpr double kDefaultAspectRatio =
      static_cast<double>(kDefaultWidth) / static_cast<double>(kDefaultHeight);

  MediaStreamVideoSource();
  ~MediaStreamVideoSource() override;

  // Returns the MediaStreamVideoSource object owned by |source|.
  static MediaStreamVideoSource* GetVideoSource(
      const blink::WebMediaStreamSource& source);

  // Puts |track| in the registered tracks list.
  void AddTrack(MediaStreamVideoTrack* track,
                const VideoTrackAdapterSettings& track_adapter_settings,
                const VideoCaptureDeliverFrameCB& frame_callback,
                const ConstraintsCallback& callback);
  void RemoveTrack(MediaStreamVideoTrack* track);

  // Called by |track| to notify the source whether it has any paths to a
  // consuming endpoint.
  void UpdateHasConsumers(MediaStreamVideoTrack* track, bool has_consumers);

  void UpdateCapturingLinkSecure(MediaStreamVideoTrack* track, bool is_secure);

  // Request underlying source to capture a new frame.
  virtual void RequestRefreshFrame() {}

  // Returns the task runner where video frames will be delivered on.
  base::SingleThreadTaskRunner* io_task_runner() const;

  // Implementations must return the capture format if available.
  virtual base::Optional<media::VideoCaptureFormat> GetCurrentFormat() const;

  base::WeakPtr<MediaStreamVideoSource> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 protected:
  void DoStopSource() override;

  // Sets ready state and notifies the ready state to all registered tracks.
  virtual void SetReadyState(blink::WebMediaStreamSource::ReadyState state);

  // Sets muted state and notifies it to all registered tracks.
  virtual void SetMutedState(bool state);

  // An implementation must start capturing frames after this method is called.
  // When the source has started or failed to start OnStartDone must be called.
  // An implementation must call |frame_callback| on the IO thread with the
  // captured frames.
  virtual void StartSourceImpl(
      const VideoCaptureDeliverFrameCB& frame_callback) = 0;
  void OnStartDone(MediaStreamRequestResult result);

  // An implementation must immediately stop capture video frames and must not
  // call OnSupportedFormats after this method has been called. After this
  // method has been called, MediaStreamVideoSource may be deleted.
  virtual void StopSourceImpl() = 0;

  // Optionally overridden by subclasses to act on whether there are any
  // consumers present. When none are present, the source can stop delivering
  // frames, giving it the option of running in an "idle" state to minimize
  // resource usage.
  virtual void OnHasConsumers(bool has_consumers) {}

  // Optionally overridden by subclasses to act on whether the capturing link
  // has become secure or insecure.
  virtual void OnCapturingLinkSecured(bool is_secure) {}

  enum State {
    NEW,
    STARTING,
    STARTED,
    ENDED
  };
  State state() const { return state_; }

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // Trigger all cached callbacks from AddTrack. AddTrack is successful
  // if the capture delegate has started and the constraints provided in
  // AddTrack match the format that was used to start the device.
  // Note that it must be ok to delete the MediaStreamVideoSource object
  // in the context of the callback. If gUM fails, the implementation will
  // simply drop the references to the blink source and track which will lead
  // to this object being deleted.
  void FinalizeAddTrack();

  State state_;

  struct TrackDescriptor {
    TrackDescriptor(MediaStreamVideoTrack* track,
                    const VideoCaptureDeliverFrameCB& frame_callback,
                    std::unique_ptr<VideoTrackAdapterSettings> adapter_settings,
                    const ConstraintsCallback& callback);
    TrackDescriptor(TrackDescriptor&& other);
    TrackDescriptor& operator=(TrackDescriptor&& other);
    ~TrackDescriptor();

    MediaStreamVideoTrack* track;
    VideoCaptureDeliverFrameCB frame_callback;
    // TODO(guidou): Make |adapter_settings| a regular field instead of a
    // unique_ptr.
    std::unique_ptr<VideoTrackAdapterSettings> adapter_settings;
    ConstraintsCallback callback;
  };
  std::vector<TrackDescriptor> track_descriptors_;

  // |track_adapter_| delivers video frames to the tracks on the IO-thread.
  const scoped_refptr<VideoTrackAdapter> track_adapter_;

  // Tracks that currently are connected to this source.
  std::vector<MediaStreamVideoTrack*> tracks_;

  // Tracks that have no paths to a consuming endpoint, and so do not need
  // frames delivered from the source. This is a subset of |tracks_|.
  std::vector<MediaStreamVideoTrack*> suspended_tracks_;

  // This is used for tracking if all connected video sinks are secure.
  SecureDisplayLinkTracker<MediaStreamVideoTrack> secure_tracker_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaStreamVideoSource> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_SOURCE_H_
