// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_OMX_VIDEO_DECODER_H_
#define MEDIA_FILTERS_OMX_VIDEO_DECODER_H_

#include <queue>

#include "media/base/filters.h"
#include "media/base/media_format.h"

class MessageLoop;

namespace media {

class Buffer;
class FilterFactory;
class OmxVideoDecodeEngine;
class VideoFrame;

class OmxVideoDecoder : public VideoDecoder {
 public:
  static FilterFactory* CreateFactory();
  static bool IsMediaFormatSupported(const MediaFormat& media_format);

  OmxVideoDecoder(OmxVideoDecodeEngine* engine);
  virtual ~OmxVideoDecoder();

  virtual void Initialize(DemuxerStream* stream, FilterCallback* callback);
  virtual void Stop(FilterCallback* callback);
  virtual void Pause(FilterCallback* callback);
  virtual void Flush(FilterCallback* callback);
  virtual void Seek(base::TimeDelta time, FilterCallback* callback);
  virtual void FillThisBuffer(scoped_refptr<VideoFrame> frame);
  virtual bool ProvidesBuffer();
  virtual const MediaFormat& media_format() { return media_format_; }

 private:
  virtual void DoInitialize(DemuxerStream* stream, FilterCallback* callback);

  // Called after the decode engine has successfully decoded something.
  void FillBufferCallback(scoped_refptr<VideoFrame> frame);

  // Called after the decode engine has consumed an input buffer.
  void EmptyBufferCallback(scoped_refptr<Buffer> buffer);

  void InitCompleteTask(FilterCallback* callback);

  void StopCompleteTask(FilterCallback* callback);
  void PauseCompleteTask(FilterCallback* callback);
  void FlushCompleteTask(FilterCallback* callback);
  void SeekCompleteTask(FilterCallback* callback);

  // TODO(hclam): This is very ugly that we keep reference instead of
  // scoped_refptr.
  void DemuxCompleteTask(Buffer* buffer);

  // Calls |omx_engine_|'s EmptyThisBuffer() method on the right thread.
  void EmptyBufferTask(scoped_refptr<Buffer> buffer);

  DemuxerStream* demuxer_stream_;
  bool supports_egl_image_;
  scoped_refptr<OmxVideoDecodeEngine> omx_engine_;
  MediaFormat media_format_;
  size_t width_;
  size_t height_;

  DISALLOW_COPY_AND_ASSIGN(OmxVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_OMX_VIDEO_DECODER_H_
