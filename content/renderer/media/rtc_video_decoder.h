// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_H_
#define CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "base/time.h"
#include "content/common/content_export.h"
#include "media/base/video_decoder.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"


namespace cricket {
class VideoFrame;
}  // namespace cricket

// RTCVideoDecoder is a media::VideoDecoder designed for rendering
// Video MediaStreamTracks,
// http://dev.w3.org/2011/webrtc/editor/getusermedia.html#mediastreamtrack
// RTCVideoDecoder implements webrtc::VideoRendererInterface in order to render
// video frames provided from a webrtc::VideoTrackInteface.
// RTCVideoDecoder register itself to the Video Track when the decoder is
// initialized and deregisters itself when it is stopped.
// Calls to webrtc::VideoTrackInterface must occur on the main render thread.
class CONTENT_EXPORT RTCVideoDecoder
    : public media::VideoDecoder,
      NON_EXPORTED_BASE(public webrtc::VideoRendererInterface) {
 public:
  RTCVideoDecoder(base::TaskRunner* video_decoder_thread,  // For video decoder
                  base::TaskRunner* main_thread,  // For accessing VideoTracks.
                  webrtc::VideoTrackInterface* video_track);

  // media::VideoDecoder implementation.
  virtual void Initialize(const scoped_refptr<media::DemuxerStream>& stream,
                          const media::PipelineStatusCB& status_cb,
                          const media::StatisticsCB& statistics_cb) OVERRIDE;
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;
  virtual void Stop(const base::Closure& closure) OVERRIDE;

  // webrtc::VideoRendererInterface implementation
  virtual void SetSize(int width, int height) OVERRIDE;
  virtual void RenderFrame(const cricket::VideoFrame* frame) OVERRIDE;

 protected:
  virtual ~RTCVideoDecoder();

 private:
  friend class RTCVideoDecoderTest;
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, Initialize_Successful);
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, DoReset);
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, DoRenderFrame);
  FRIEND_TEST_ALL_PREFIXES(RTCVideoDecoderTest, DoSetSize);

  enum DecoderState {
    kUnInitialized,
    kNormal,
    kStopped
  };

  void CancelPendingRead();

  // Register this RTCVideoDecoder to get VideoFrames from the
  // webrtc::VideoTrack in |video_track_|.
  // This is done on the |main_thread_|.
  void RegisterToVideoTrack();
  void DeregisterFromVideoTrack();

  scoped_refptr<base::TaskRunner> video_decoder_thread_;
  scoped_refptr<base::TaskRunner> main_thread_;
  gfx::Size visible_size_;
  std::string url_;
  DecoderState state_;
  ReadCB read_cb_;
  bool got_first_frame_;
  base::TimeDelta last_frame_timestamp_;
  base::TimeDelta start_time_;
  // The video track the renderer is connected to.
  scoped_refptr<webrtc::VideoTrackInterface> video_track_;

  // Used for accessing |read_cb_| from another thread.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoder);
};

#endif  // CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_H_
