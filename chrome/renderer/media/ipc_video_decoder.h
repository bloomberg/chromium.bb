// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_IPC_VIDEO_DECODER_H_
#define CHROME_RENDERER_MEDIA_IPC_VIDEO_DECODER_H_

#include "base/time.h"
#include "chrome/renderer/gpu_video_service_host.h"
#include "media/base/pts_heap.h"
#include "media/base/video_frame.h"
#include "media/filters/decoder_base.h"

struct AVRational;

namespace media {

class VideoDecodeEngine;

class IpcVideoDecoder : public VideoDecoder,
                        public GpuVideoDecoderHost::EventHandler {
 public:
  explicit IpcVideoDecoder(MessageLoop* message_loop);
  virtual ~IpcVideoDecoder();

  static FilterFactory* CreateFactory(MessageLoop* message_loop);
  static bool IsMediaFormatSupported(const MediaFormat& media_format);

  // MediaFilter implementation.
  virtual void Stop(FilterCallback* callback);
  virtual void Seek(base::TimeDelta time, FilterCallback* callback);
  virtual void Pause(FilterCallback* callback);
  virtual void Flush(FilterCallback* callback);

  // Decoder implementation.
  virtual void Initialize(DemuxerStream* demuxer_stream,
                          FilterCallback* callback);
  virtual const MediaFormat& media_format() { return media_format_; }
  virtual void FillThisBuffer(scoped_refptr<VideoFrame> video_frame);

  // GpuVideoDecoderHost::EventHandler.
  virtual void OnInitializeDone(bool success,
                                const GpuVideoDecoderInitDoneParam& param);
  virtual void OnUninitializeDone();
  virtual void OnFlushDone();
  virtual void OnEmptyBufferDone(scoped_refptr<Buffer> buffer);
  virtual void OnFillBufferDone(scoped_refptr<VideoFrame> frame);
  virtual void OnDeviceError();

  virtual bool ProvidesBuffer();

 private:
  friend class FilterFactoryImpl2<IpcVideoDecoder,
                                  VideoDecodeEngine*,
                                  MessageLoop*>;

  enum DecoderState {
    kUnInitialized,
    kPlaying,
    kFlushing,
    kPausing,
    kFlushCodec,
    kEnded,
    kStopped,
  };

  void OnSeekComplete(FilterCallback* callback);
  void OnReadComplete(Buffer* buffer);
  void ReadCompleteTask(scoped_refptr<Buffer> buffer);

  int32 width_;
  int32 height_;
  MediaFormat media_format_;

  scoped_ptr<FilterCallback> flush_callback_;
  scoped_ptr<FilterCallback> initialize_callback_;
  scoped_ptr<FilterCallback> stop_callback_;

  DecoderState state_;

  // Tracks the number of asynchronous reads issued to |demuxer_stream_|.
  // Using size_t since it is always compared against deque::size().
  size_t pending_reads_;
  // Tracks the number of asynchronous reads issued from renderer.
  size_t pending_requests_;

  // Pointer to the demuxer stream that will feed us compressed buffers.
  scoped_refptr<DemuxerStream> demuxer_stream_;

  MessageLoop* renderer_thread_message_loop_;
  scoped_refptr<GpuVideoDecoderHost> gpu_video_decoder_host_;

  DISALLOW_COPY_AND_ASSIGN(IpcVideoDecoder);
};

}  // namespace media

#endif  // CHROME_RENDERER_MEDIA_IPC_VIDEO_DECODER_H_
