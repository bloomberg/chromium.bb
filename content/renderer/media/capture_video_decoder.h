// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CAPTURE_VIDEO_DECODER_H_
#define CONTENT_RENDERER_MEDIA_CAPTURE_VIDEO_DECODER_H_

#include "base/time.h"
#include "content/common/content_export.h"
#include "media/base/demuxer_stream.h"
#include "media/base/filters.h"
#include "media/base/video_frame.h"
#include "media/video/capture/video_capture.h"

namespace base {
class MessageLoopProxy;
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
      const media::VideoCapture::VideoCaptureCapability& capability);
  virtual ~CaptureVideoDecoder();

  // Filter implementation.
  virtual void Play(const base::Closure& callback) OVERRIDE;
  virtual void Seek(base::TimeDelta time,
                    const media::FilterStatusCB& cb) OVERRIDE;
  virtual void Pause(const base::Closure& callback) OVERRIDE;
  virtual void Flush(const base::Closure& callback) OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;

  // Decoder implementation.
  virtual void Initialize(
      media::DemuxerStream* demuxer_stream,
      const base::Closure& filter_callback,
      const media::StatisticsCallback& stat_callback) OVERRIDE;
  virtual void Read(const ReadCB& callback) OVERRIDE;
  virtual const gfx::Size& natural_size() OVERRIDE;

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

 private:
  friend class CaptureVideoDecoderTest;

  enum DecoderState {
    kUnInitialized,
    kNormal,
    kStopped,
    kPaused
  };

  void PlayOnDecoderThread(const base::Closure& callback);
  void SeekOnDecoderThread(base::TimeDelta time,
                           const media::FilterStatusCB& cb);
  void PauseOnDecoderThread(const base::Closure& callback);
  void FlushOnDecoderThread(const base::Closure& callback);
  void StopOnDecoderThread(const base::Closure& callback);

  void InitializeOnDecoderThread(
      media::DemuxerStream* demuxer_stream,
      const base::Closure& filter_callback,
      const media::StatisticsCallback& stat_callback);
  void ReadOnDecoderThread(const ReadCB& callback);

  void OnStoppedOnDecoderThread(media::VideoCapture* capture);
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
  media::VideoCapture::VideoCaptureCapability capability_;
  gfx::Size natural_size_;
  DecoderState state_;
  ReadCB read_cb_;
  base::Closure pending_stop_cb_;
  media::StatisticsCallback statistics_callback_;

  media::VideoCaptureSessionId video_stream_id_;
  media::VideoCapture* capture_engine_;
  base::Time start_time_;

  DISALLOW_COPY_AND_ASSIGN(CaptureVideoDecoder);
};

#endif  // CONTENT_RENDERER_MEDIA_CAPTURE_VIDEO_DECODER_H_
