// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_VIDEO_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_VIDEO_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {

// MediaStreamRemoteVideoSource implements the MediaStreamVideoSource interface
// for video tracks received on a PeerConnection. The purpose of the class is
// to make sure there is no difference between a video track where the source is
// a local source and a video track where the source is a remote video track.
class CONTENT_EXPORT MediaStreamRemoteVideoSource
     : public MediaStreamVideoSource {
 public:
  class CONTENT_EXPORT Observer
      : public base::RefCountedThreadSafe<Observer>,
        NON_EXPORTED_BASE(public webrtc::ObserverInterface) {
   public:
    Observer(const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
             webrtc::VideoTrackInterface* track);

    const scoped_refptr<webrtc::VideoTrackInterface>& track();
    webrtc::MediaStreamTrackInterface::TrackState state() const;

    // This needs to be called by the owner of the observer instance before
    // the owner releases its reference.
    // The reason for this is to avoid a potential race when unregistration is
    // done from the main thread while an event is being delivered on the
    // signaling thread.  If, on the main thread, we're releasing the last
    // reference to the observer and attempt to unregister from the observer's
    // dtor, and at the same time receive an OnChanged event on the signaling
    // thread, we will attempt to increment the refcount in the callback
    // from 0 to 1 while the object is being freed.  Not good.
    void Unregister();

   private:
    friend class base::RefCountedThreadSafe<Observer>;
    ~Observer() override;

    friend class MediaStreamRemoteVideoSource;
    void SetSource(const base::WeakPtr<MediaStreamRemoteVideoSource>& source);

    // webrtc::ObserverInterface implementation.
    void OnChanged() override;

    void OnChangedImpl(webrtc::MediaStreamTrackInterface::TrackState state);

    const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
    base::WeakPtr<MediaStreamRemoteVideoSource> source_;
#if DCHECK_IS_ON
    bool source_set_;
#endif
    scoped_refptr<webrtc::VideoTrackInterface> track_;
    webrtc::MediaStreamTrackInterface::TrackState state_;
  };

  MediaStreamRemoteVideoSource(const scoped_refptr<Observer>& observer);
  virtual ~MediaStreamRemoteVideoSource();

 protected:
  // Implements MediaStreamVideoSource.
  void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height,
      double max_requested_frame_rate,
      const VideoCaptureDeviceFormatsCB& callback) override;

  void StartSourceImpl(
      const media::VideoCaptureFormat& format,
      const VideoCaptureDeliverFrameCB& frame_callback) override;

  void StopSourceImpl() override;

  // Used by tests to test that a frame can be received and that the
  // MediaStreamRemoteVideoSource behaves as expected.
  webrtc::VideoRendererInterface* RenderInterfaceForTest();

 private:
  friend class Observer;
  void OnChanged(webrtc::MediaStreamTrackInterface::TrackState state);

  scoped_refptr<Observer> observer_;

  // Internal class used for receiving frames from the webrtc track on a
  // libjingle thread and forward it to the IO-thread.
  class RemoteVideoSourceDelegate;
  scoped_refptr<RemoteVideoSourceDelegate> delegate_;
  base::WeakPtrFactory<MediaStreamRemoteVideoSource> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamRemoteVideoSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_MEDIA_STREAM_REMOTE_VIDEO_SOURCE_H_
