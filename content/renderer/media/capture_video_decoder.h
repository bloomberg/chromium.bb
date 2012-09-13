// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CAPTURE_VIDEO_DECODER_H_
#define CONTENT_RENDERER_MEDIA_CAPTURE_VIDEO_DECODER_H_

#include "base/time.h"
#include "content/common/content_export.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder.h"
#include "media/video/capture/video_capture.h"
#include "media/video/capture/video_capture_types.h"

namespace base {
class MessageLoopProxy;
}
namespace media {
class VideoFrame;
}
class VideoCaptureImplManager;

// A filter takes raw frames from video capture engine and passes them to media
// engine as a video decoder filter.
class CONTENT_EXPORT CaptureVideoDecoder
    : public media::VideoDecoder,
      public media::VideoCapture::EventHandler {
 public:
  CaptureVideoDecoder(
      base::MessageLoopProxy* message_loop_proxy,
      media::VideoCaptureSessionId video_stream_id,
      VideoCaptureImplManager* vc_manager,
      const media::VideoCaptureCapability& capability);

  // media::VideoDecoder implementation.
  virtual void Initialize(const scoped_refptr<media::DemuxerStream>& stream,
                          const media::PipelineStatusCB& status_cb,
                          const media::StatisticsCB& statistics_cb) OVERRIDE;
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;
  virtual void Stop(const base::Closure& closure) OVERRIDE;

  // VideoCapture::EventHandler implementation.
  virtual void OnStarted(media::VideoCapture* capture) OVERRIDE;
  virtual void OnStopped(media::VideoCapture* capture) OVERRIDE;
  virtual void OnPaused(media::VideoCapture* capture) OVERRIDE;
  virtual void OnError(media::VideoCapture* capture, int error_code) OVERRIDE;
  virtual void OnRemoved(media::VideoCapture* capture) OVERRIDE;
  virtual void OnBufferReady(
      media::VideoCapture* capture,
      scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf) OVERRIDE;
  virtual void OnDeviceInfoReceived(
      media::VideoCapture* capture,
      const media::VideoCaptureParams& device_info) OVERRIDE;

 protected:
  virtual ~CaptureVideoDecoder();

 private:
  friend class CaptureVideoDecoderTest;

  enum DecoderState {
    kUnInitialized,
    kNormal,
    kStopped,
    kPaused
  };

  void InitializeOnDecoderThread(
      const scoped_refptr<media::DemuxerStream>& stream,
      const media::PipelineStatusCB& status_cb,
      const media::StatisticsCB& statistics_cb);
  void ReadOnDecoderThread(const ReadCB& read_cb);
  void ResetOnDecoderThread(const base::Closure& closure);
  void StopOnDecoderThread(const base::Closure& closure);
  void PrepareForShutdownHackOnDecoderThread();

  void OnStoppedOnDecoderThread(media::VideoCapture* capture);
  void OnPausedOnDecoderThread(media::VideoCapture* capture);
  void OnBufferReadyOnDecoderThread(
      media::VideoCapture* capture,
      scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf);
  void OnDeviceInfoReceivedOnDecoderThread(
      media::VideoCapture* capture,
      const media::VideoCaptureParams& device_info);

  // Delivers the frame to |read_cb_| and resets the callback.
  void DeliverFrame(const scoped_refptr<media::VideoFrame>& video_frame);

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  scoped_refptr<VideoCaptureImplManager> vc_manager_;
  media::VideoCaptureCapability capability_;
  gfx::Size natural_size_;
  DecoderState state_;
  bool got_first_frame_;
  ReadCB read_cb_;
  base::Closure pending_stop_cb_;
  media::StatisticsCB statistics_cb_;

  media::VideoCaptureSessionId video_stream_id_;
  media::VideoCapture* capture_engine_;
  base::Time last_frame_timestamp_;
  base::Time start_time_;

  DISALLOW_COPY_AND_ASSIGN(CaptureVideoDecoder);
};

#endif  // CONTENT_RENDERER_MEDIA_CAPTURE_VIDEO_DECODER_H_
