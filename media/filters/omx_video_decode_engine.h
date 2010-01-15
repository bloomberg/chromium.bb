// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_OMX_VIDEO_DECODE_ENGINE_H_
#define MEDIA_FILTERS_OMX_VIDEO_DECODE_ENGINE_H_

#include <list>

#include "base/lock.h"
#include "base/task.h"
#include "media/filters/video_decode_engine.h"
#include "media/omx/input_buffer.h"
#include "media/omx/omx_codec.h"

class MessageLoop;

// FFmpeg types.
struct AVFrame;
struct AVRational;
struct AVStream;

namespace media {

class OmxVideoDecodeEngine : public VideoDecodeEngine {
 public:
  OmxVideoDecodeEngine();
  virtual ~OmxVideoDecodeEngine();

  // Implementation of the VideoDecodeEngine Interface.
  virtual void Initialize(AVStream* stream, Task* done_cb);
  virtual void DecodeFrame(const Buffer& buffer, AVFrame* yuv_frame,
                           bool* got_result, Task* done_cb);
  virtual void Flush(Task* done_cb);
  virtual VideoSurface::Format GetSurfaceFormat() const;

  virtual State state() const { return state_; }

  // Stops the engine.
  //
  // TODO(ajwong): Normalize this interface with Task like the others, and
  // promote to the abstract interface.
  virtual void Stop(Callback0::Type* done_cb);

  virtual void set_message_loop(MessageLoop* message_loop) {
    message_loop_ = message_loop;
  };

 private:
  struct YuvFrame {
    // TODO(ajwong): Please avoid ever using this class anywhere else until
    // we've consolidated the buffer struct.
    YuvFrame(size_t c) {
      size = 0;
      capacity = c;
      data = new unsigned char[capacity];
    }
    ~YuvFrame() {
      delete [] data;
    }
    size_t size;
    size_t capacity;
    unsigned char* data;
  };

  virtual void OnFeedDone(InputBuffer* buffer);
  virtual void OnHardwareError();
  virtual void OnReadComplete(uint8* buffer, int size);
  virtual void OnFormatChange(OmxCodec::OmxMediaFormat* input_format,
                              OmxCodec::OmxMediaFormat* output_format);

  virtual bool DecodedFrameAvailable();
  virtual void MergeBytesFrameQueue(uint8* buffer, int size);
  virtual bool IsFrameComplete(const YuvFrame* frame);
  virtual YuvFrame* GetFrame();

  Lock lock_;  // Locks the |state_| variable and the |yuv_frame_queue_|.
  State state_;
  size_t frame_bytes_;
  size_t width_;
  size_t height_;

  bool has_fed_on_eos_;  // Used to avoid sending an end of stream to
                         // OpenMax twice since OpenMax does not always
                         // handle this nicely.
  std::list<YuvFrame*> yuv_frame_queue_;

  scoped_refptr<media::OmxCodec> omx_codec_;
  MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(OmxVideoDecodeEngine);
};

}  // namespace media

#endif  // MEDIA_FILTERS_OMX_VIDEO_DECODE_ENGINE_H_
