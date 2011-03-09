// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_DECODE_ENGINE_H_
#define MEDIA_VIDEO_VIDEO_DECODE_ENGINE_H_

#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "media/base/video_frame.h"

class MessageLoop;

namespace media {

class Buffer;
class VideoDecodeContext;

struct PipelineStatistics;

enum VideoCodec {
  kUnknown,
  kCodecH264,
  kCodecVC1,
  kCodecMPEG2,
  kCodecMPEG4,
  kCodecTheora,
  kCodecVP8,
};

class VideoCodecConfig {
 public:
  VideoCodecConfig(VideoCodec codec, int width, int height,
                   int frame_rate_numerator, int frame_rate_denominator,
                   uint8* extra_data, size_t extra_data_size);
  ~VideoCodecConfig();

  VideoCodec codec() const;
  int width() const;
  int height() const;
  int frame_rate_numerator() const;
  int frame_rate_denominator() const;
  uint8* extra_data() const;
  size_t extra_data_size() const;

 private:
  VideoCodec codec_;

  // Container's concept of width and height of this video.
  int width_;
  int height_;

  // Frame rate in seconds expressed as a fraction.
  // TODO(scherkus): fairly certain decoders don't require frame rates.
  int frame_rate_numerator_;
  int frame_rate_denominator_;

  // Optional byte data requied to initialize video decoders.
  scoped_array<uint8> extra_data_;
  size_t extra_data_size_;

  DISALLOW_COPY_AND_ASSIGN(VideoCodecConfig);
};

struct VideoStreamInfo {
  VideoFrame::Format surface_format;
  VideoFrame::SurfaceType surface_type;

  // Can be different with container's value.
  uint32 surface_width;

  // Can be different with container's value.
  uint32 surface_height;
};

struct VideoCodecInfo {
  // Other parameter is only meaningful when this is true.
  bool success;

  // Whether decoder provides output buffer pool.
  // TODO(hclam): This is not important anymore. Remove this.
  bool provides_buffers;

  // Initial Stream Info. Only part of them could be valid.
  // If they are not valid, Engine should update with OnFormatChange.
  VideoStreamInfo stream_info;
};

class VideoDecodeEngine {
 public:
  struct EventHandler {
   public:
    virtual ~EventHandler() {}
    virtual void OnInitializeComplete(const VideoCodecInfo& info) = 0;
    virtual void OnUninitializeComplete() = 0;
    virtual void OnFlushComplete() = 0;
    virtual void OnSeekComplete() = 0;
    virtual void OnError() = 0;
    virtual void OnFormatChange(VideoStreamInfo stream_info) = 0;

    // TODO(hclam): The following two methods shouldn't belong to this class
    // because they are not video decode events but used to send decoded
    // video frames and request video packets.
    //
    // Signal the user of VideoDecodeEngine to provide a video sample.
    //
    // In the normal running state, this method is called by the video decode
    // engine to request video samples used for decoding.
    //
    // In the case when the video decode engine is flushing, this method is
    // called to return video samples acquired by the video decode engine.
    //
    // |buffer| can be NULL in which case this method call is purely for
    // requesting new video samples. If |buffer| is non-NULL, the buffer is
    // returned to the owner at the sample time as a request for video sample
    // is made.
    virtual void ProduceVideoSample(scoped_refptr<Buffer> buffer) = 0;

    // Signal the user of VideoDecodeEngine that a video frame is ready to
    // be consumed or a video frame is returned to the owner.
    //
    // In the normal running state, this method is called to signal that
    // |frame| contains a decoded video frame and is ready to be used.
    //
    // In the case of flushing and video frame is provided externally, this
    // method is called to return the video frame object to the owner.
    // The content of the video frame may be invalid.
    virtual void ConsumeVideoFrame(scoped_refptr<VideoFrame> frame,
                                   const PipelineStatistics& statistics) = 0;
  };

  virtual ~VideoDecodeEngine() {}

  // Initialize the engine with specified configuration.
  //
  // |decode_context| is used for allocation of VideoFrame.
  // It is important that |decode_context| is called only on |message_loop|.
  //
  // TODO(hclam): Currently refactoring code to use VideoDecodeContext so
  // |context| may be NULL in some cases.
  //
  // Engine should call EventHandler::OnInitializeDone() whether the
  // initialization operation finished successfully or not.
  virtual void Initialize(MessageLoop* message_loop,
                          EventHandler* event_handler,
                          VideoDecodeContext* context,
                          const VideoCodecConfig& config) = 0;

  // Uninitialize the engine. Engine should destroy all resources and call
  // EventHandler::OnUninitializeComplete().
  virtual void Uninitialize() = 0;

  // Flush the engine. Engine should return all the buffers to owner ( which
  // could be itself. ) then call EventHandler::OnFlushDone().
  virtual void Flush() = 0;

  // This method is used as a signal for the decode engine to prefoll and
  // issue read requests after Flush() us made.
  virtual void Seek() = 0;

  // Provide a video sample to be used by the video decode engine.
  //
  // This method is called in response to ProvideVideoSample() called to the
  // user.
  virtual void ConsumeVideoSample(scoped_refptr<Buffer> buffer) = 0;

  // Signal the video decode engine to produce a video frame or return the
  // video frame object to the video decode engine.
  //
  // In the normal running state, this method is called by the user of the
  // video decode engine to request a decoded video frame. If |frame| is
  // NULL the video decode engine should allocate a video frame object.
  // Otherwise video decode engine should try to use the video frame object
  // provided as output.
  //
  // In flushing state and video frames are allocated internally this method
  // is called by the user to return the video frame object.
  //
  // In response to this method call, ConsumeVideoFrame() is called with a
  // video frame object containing decoded video content.
  virtual void ProduceVideoFrame(scoped_refptr<VideoFrame> frame) = 0;
};

}  // namespace media

#endif  // MEDIA_VIDEO_VIDEO_DECODE_ENGINE_H_
