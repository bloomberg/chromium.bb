// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_OMX_VIDEO_DECODER_H_
#define MEDIA_FILTERS_OMX_VIDEO_DECODER_H_

#include "media/filters/video_decoder_impl.h"

class MessageLoop;

namespace media {

class FilterFactory;
class MediaFormat;
class OmxVideoDecodeEngine;

class OmxVideoDecoder : public VideoDecoderImpl {
 public:
  static FilterFactory* CreateFactory();
  static bool IsMediaFormatSupported(const MediaFormat& media_format);

  OmxVideoDecoder(OmxVideoDecodeEngine* engine);
  virtual ~OmxVideoDecoder();

  virtual void set_message_loop(MessageLoop* message_loop);
  virtual void Stop();

 private:
  OmxVideoDecodeEngine* omx_engine_;

  DISALLOW_COPY_AND_ASSIGN(OmxVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_OMX_VIDEO_DECODER_H_
