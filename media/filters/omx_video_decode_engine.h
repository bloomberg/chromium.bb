// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_OMX_VIDEO_DECODE_ENGINE_H_
#define MEDIA_FILTERS_OMX_VIDEO_DECODE_ENGINE_H_

#include <functional>
#include <list>
#include <vector>

#include "base/callback.h"
#include "base/lock.h"
#include "base/task.h"
#include "media/filters/video_decode_engine.h"
#include "media/omx/omx_codec.h"

class MessageLoop;

// FFmpeg types.
struct AVRational;
struct AVStream;

namespace media {

// TODO(hclam):
// Extract the OmxOutputSink implementation to a strategy class.
class OmxVideoDecodeEngine : public VideoDecodeEngine {
 public:
  OmxVideoDecodeEngine();
  virtual ~OmxVideoDecodeEngine();

  // Implementation of the VideoDecodeEngine Interface.
  virtual void Initialize(AVStream* stream, Task* done_cb);
  virtual void DecodeFrame(Buffer* buffer,
                           scoped_refptr<VideoFrame>* video_frame,
                           bool* got_result, Task* done_cb);
  virtual void Flush(Task* done_cb);
  virtual VideoFrame::Format GetSurfaceFormat() const;

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
  // A struct to hold parameters of a decode request. Objects pointed by
  // these parameters are owned by the caller.
  struct DecodeRequest {
    DecodeRequest(scoped_refptr<VideoFrame>* f, bool* b, Task* cb)
        : frame(f),
          got_result(b),
          done_cb(cb) {
    }

    scoped_refptr<VideoFrame>* frame;
    bool* got_result;
    Task* done_cb;
  };

  virtual void OnFeedDone(Buffer* buffer);
  virtual void OnHardwareError();
  virtual void OnReadComplete(OMX_BUFFERHEADERTYPE* buffer);
  virtual void OnFormatChange(
      const OmxConfigurator::MediaFormat& input_format,
      const OmxConfigurator::MediaFormat& output_format);

  State state_;
  size_t width_;
  size_t height_;

  // TODO(hclam): We should let OmxCodec handle this case.
  bool has_fed_on_eos_;  // Used to avoid sending an end of stream to
  // OpenMax twice since OpenMax does not always
  // handle this nicely.
  std::list<DecodeRequest> decode_request_queue_;

  scoped_refptr<media::OmxCodec> omx_codec_;
  scoped_ptr<media::OmxConfigurator> omx_configurator_;
  MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(OmxVideoDecodeEngine);
};

}  // namespace media

#endif  // MEDIA_FILTERS_OMX_VIDEO_DECODE_ENGINE_H_
