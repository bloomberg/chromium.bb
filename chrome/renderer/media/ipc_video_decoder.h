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

namespace ggl {
class Context;
}  // namespace ggl

class IpcVideoDecoder : public media::VideoDecoder,
                        public GpuVideoDecoderHost::EventHandler {
 public:
  explicit IpcVideoDecoder(MessageLoop* message_loop,
                           ggl::Context* ggl_context);
  virtual ~IpcVideoDecoder();

  static media::FilterFactory* CreateFactory(MessageLoop* message_loop,
                                             ggl::Context* ggl_context);
  static bool IsMediaFormatSupported(const media::MediaFormat& media_format);

  // MediaFilter implementation.
  virtual void Stop(media::FilterCallback* callback);
  virtual void Seek(base::TimeDelta time, media::FilterCallback* callback);
  virtual void Pause(media::FilterCallback* callback);
  virtual void Flush(media::FilterCallback* callback);

  // Decoder implementation.
  virtual void Initialize(media::DemuxerStream* demuxer_stream,
                          media::FilterCallback* callback);
  virtual const media::MediaFormat& media_format() { return media_format_; }
  virtual void ProduceVideoFrame(scoped_refptr<media::VideoFrame> video_frame);

  // GpuVideoDecoderHost::EventHandler.
  virtual void OnInitializeDone(bool success,
                                const GpuVideoDecoderInitDoneParam& param);
  virtual void OnUninitializeDone();
  virtual void OnFlushDone();
  virtual void OnEmptyBufferDone(scoped_refptr<media::Buffer> buffer);
  virtual void OnFillBufferDone(scoped_refptr<media::VideoFrame> frame);
  virtual void OnDeviceError();

  virtual bool ProvidesBuffer();

 private:
  enum DecoderState {
    kUnInitialized,
    kPlaying,
    kFlushing,
    kPausing,
    kFlushCodec,
    kEnded,
    kStopped,
  };

  void OnSeekComplete(media::FilterCallback* callback);
  void OnReadComplete(media::Buffer* buffer);
  void ReadCompleteTask(scoped_refptr<media::Buffer> buffer);

  int32 width_;
  int32 height_;
  media::MediaFormat media_format_;

  scoped_ptr<media::FilterCallback> flush_callback_;
  scoped_ptr<media::FilterCallback> initialize_callback_;
  scoped_ptr<media::FilterCallback> stop_callback_;

  DecoderState state_;

  // Tracks the number of asynchronous reads issued to |demuxer_stream_|.
  // Using size_t since it is always compared against deque::size().
  size_t pending_reads_;
  // Tracks the number of asynchronous reads issued from renderer.
  size_t pending_requests_;

  // Pointer to the demuxer stream that will feed us compressed buffers.
  scoped_refptr<media::DemuxerStream> demuxer_stream_;

  MessageLoop* renderer_thread_message_loop_;

  // A context for allocating textures and issuing GLES2 commands.
  // TODO(hclam): A ggl::Context lives on the Render Thread while this object
  // lives on the Video Decoder Thread, we need to take care of context lost
  // and destruction of the context.
  ggl::Context* ggl_context_;

  scoped_refptr<GpuVideoDecoderHost> gpu_video_decoder_host_;

  DISALLOW_COPY_AND_ASSIGN(IpcVideoDecoder);
};

#endif  // CHROME_RENDERER_MEDIA_IPC_VIDEO_DECODER_H_
