// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface and some concrete classes for applying various transforms
// to AVPackets.  FFmpegBitstreamConverter, in particular, can be used
// to apply FFmpeg bitstream filters to the incoming AVPacket to transcode
// the packet format.

#ifndef MEDIA_FILTERS_BITSTREAM_CONVERTER_H_
#define MEDIA_FILTERS_BITSTREAM_CONVERTER_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "media/base/media_export.h"

// FFmpeg types.
struct AVBitStreamFilterContext;
struct AVCodecContext;
struct AVPacket;

namespace media {

class MEDIA_EXPORT BitstreamConverter {
 public:
  BitstreamConverter() {}
  virtual ~BitstreamConverter() {}

  // Initialize does any preparations needed before doing the actual
  // conversion.
  virtual bool Initialize() = 0;

  // Attemps to convert the AVPacket from one format to another, based on the
  // specific type of BitstreamConverter that was instantiated. Output will be
  // stored into the |packet|, but user should be aware that conversion can free
  // and reallocate the input buffer, if it needs to do so to fit it in.
  virtual bool ConvertPacket(AVPacket* packet) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BitstreamConverter);
};

class IdentityBitstreamConverter : public BitstreamConverter {
 public:
  IdentityBitstreamConverter() {}
  virtual ~IdentityBitstreamConverter() {}

  virtual bool Initialize();
  virtual bool ConvertPacket(AVPacket* packet);

 private:
  DISALLOW_COPY_AND_ASSIGN(IdentityBitstreamConverter);
};

class MEDIA_EXPORT FFmpegBitstreamConverter : public BitstreamConverter {
 public:
  // Creates FFmpegBitstreamConverter based on the FFmpeg bistream filter
  // corresponding to |filter_name|.
  //
  // The |stream_context| will be used during conversion and should be the
  // AVCodecContext for the stream sourcing these packets. A reference to
  // |stream_context| is retained, so it must outlive this class.
  FFmpegBitstreamConverter(const std::string& filter_name,
                           AVCodecContext* stream_context);
  virtual ~FFmpegBitstreamConverter();

  virtual bool Initialize();
  virtual bool ConvertPacket(AVPacket* packet);

 private:
  std::string filter_name_;
  AVBitStreamFilterContext* stream_filter_;
  AVCodecContext* stream_context_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegBitstreamConverter);
};

}  // namespace media

#endif  // MEDIA_FILTERS_BITSTREAM_CONVERTER_H_
