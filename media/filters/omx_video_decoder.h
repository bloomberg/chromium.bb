// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_OMX_VIDEO_DECODER_H_
#define MEDIA_FILTERS_OMX_VIDEO_DECODER_H_

#include <queue>

#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/base/media_format.h"
#include "media/video/video_decode_engine.h"

class MessageLoop;

namespace media {

class Buffer;
class VideoFrame;

class OmxVideoDecoder : public VideoDecoder,
                        public VideoDecodeEngine::EventHandler {
 public:
  static FilterFactory* CreateFactory();
  static bool IsMediaFormatSupported(const MediaFormat& media_format);

  OmxVideoDecoder(VideoDecodeEngine* engine);
  virtual ~OmxVideoDecoder();

  virtual void Initialize(DemuxerStream* stream, FilterCallback* callback);
  virtual void Stop(FilterCallback* callback);
  virtual void Flush(FilterCallback* callback);
  virtual void Seek(base::TimeDelta time, FilterCallback* callback);
  virtual void FillThisBuffer(scoped_refptr<VideoFrame> frame);
  virtual bool ProvidesBuffer();
  virtual const MediaFormat& media_format() { return media_format_; }

 private:
  // VideoDecodeEngine::EventHandler interface.
  virtual void OnInitializeComplete(const VideoCodecInfo& info);
  virtual void OnUninitializeComplete();
  virtual void OnFlushComplete();
  virtual void OnSeekComplete();
  virtual void OnError();
  virtual void OnFormatChange(VideoStreamInfo stream_info);
  virtual void OnEmptyBufferCallback(scoped_refptr<Buffer> buffer);
  virtual void OnFillBufferCallback(scoped_refptr<VideoFrame> frame);

  // TODO(hclam): This is very ugly that we keep reference instead of
  // scoped_refptr.
  void DemuxCompleteTask(Buffer* buffer);

  // Pointer to the demuxer stream that will feed us compressed buffers.
  scoped_refptr<DemuxerStream> demuxer_stream_;
  scoped_refptr<VideoDecodeEngine> omx_engine_;
  MediaFormat media_format_;
  size_t width_;
  size_t height_;

  scoped_ptr<FilterCallback> initialize_callback_;
  scoped_ptr<FilterCallback> uninitialize_callback_;
  scoped_ptr<FilterCallback> flush_callback_;
  scoped_ptr<FilterCallback> seek_callback_;

  VideoCodecInfo info_;

  DISALLOW_COPY_AND_ASSIGN(OmxVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_OMX_VIDEO_DECODER_H_
