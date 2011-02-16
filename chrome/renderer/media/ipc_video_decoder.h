// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_IPC_VIDEO_DECODER_H_
#define CHROME_RENDERER_MEDIA_IPC_VIDEO_DECODER_H_

#include "base/time.h"
#include "media/base/pts_heap.h"
#include "media/base/video_frame.h"
#include "media/filters/decoder_base.h"
#include "media/video/video_decode_engine.h"
#include "media/video/video_decode_context.h"

struct AVRational;

namespace ggl {
class Context;
}  // namespace ggl

class IpcVideoDecoder : public media::VideoDecoder,
                        public media::VideoDecodeEngine::EventHandler {
 public:
  IpcVideoDecoder(MessageLoop* message_loop, ggl::Context* ggl_context);
  virtual ~IpcVideoDecoder();

  // media::Filter implementation.
  virtual void Stop(media::FilterCallback* callback);
  virtual void Seek(base::TimeDelta time, media::FilterCallback* callback);
  virtual void Pause(media::FilterCallback* callback);
  virtual void Flush(media::FilterCallback* callback);

  // media::VideoDecoder implementation.
  virtual void Initialize(media::DemuxerStream* demuxer_stream,
                          media::FilterCallback* callback,
                          media::StatisticsCallback* statsCallback);
  virtual const media::MediaFormat& media_format();
  virtual void ProduceVideoFrame(scoped_refptr<media::VideoFrame> video_frame);

  // TODO(hclam): Remove this method.
  virtual bool ProvidesBuffer();

  // media::VideoDecodeEngine::EventHandler implementation.
  virtual void OnInitializeComplete(const media::VideoCodecInfo& info);
  virtual void OnUninitializeComplete();
  virtual void OnFlushComplete();
  virtual void OnSeekComplete();
  virtual void OnError();

  // TODO(hclam): Remove this method.
  virtual void OnFormatChange(media::VideoStreamInfo stream_info) {}
  virtual void ProduceVideoSample(scoped_refptr<media::Buffer> buffer);
  virtual void ConsumeVideoFrame(scoped_refptr<media::VideoFrame> frame,
                                 const media::PipelineStatistics& statistics);

 private:
  void OnReadComplete(media::Buffer* buffer);
  void OnDestroyComplete();

  int32 width_;
  int32 height_;
  media::MediaFormat media_format_;

  scoped_ptr<media::FilterCallback> flush_callback_;
  scoped_ptr<media::FilterCallback> seek_callback_;
  scoped_ptr<media::FilterCallback> initialize_callback_;
  scoped_ptr<media::FilterCallback> stop_callback_;
  scoped_ptr<media::StatisticsCallback> statistics_callback_;

  // Pointer to the demuxer stream that will feed us compressed buffers.
  scoped_refptr<media::DemuxerStream> demuxer_stream_;

  // This is the message loop that we should assign to VideoDecodeContext.
  MessageLoop* decode_context_message_loop_;

  // A context for allocating textures and issuing GLES2 commands.
  // TODO(hclam): A ggl::Context lives on the Render Thread while this object
  // lives on the Video Decoder Thread, we need to take care of context lost
  // and destruction of the context.
  ggl::Context* ggl_context_;

  // This VideoDecodeEngine translate our requests to IPC commands to the
  // GPU process.
  // VideoDecodeEngine should run on IO Thread instead of Render Thread to
  // avoid dead lock during tear down of the media pipeline.
  scoped_ptr<media::VideoDecodeEngine> decode_engine_;

  // Decoding context to be used by VideoDecodeEngine.
  scoped_ptr<media::VideoDecodeContext> decode_context_;

  DISALLOW_COPY_AND_ASSIGN(IpcVideoDecoder);
};

#endif  // CHROME_RENDERER_MEDIA_IPC_VIDEO_DECODER_H_
