// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/decoding_image_generator.h"

#include "blimp/common/compositor/webp_decoder.h"
#include "third_party/libwebp/webp/decode.h"
#include "third_party/libwebp/webp/demux.h"
#include "third_party/skia/include/core/SkData.h"

namespace blimp {
namespace client {

SkImageGenerator* DecodingImageGenerator::create(SkData* data) {
  WebPData inputData = {reinterpret_cast<const uint8_t*>(data->data()),
                        data->size()};
  WebPDemuxState demuxState(WEBP_DEMUX_PARSING_HEADER);
  WebPDemuxer* demux = WebPDemuxPartial(&inputData, &demuxState);

  uint32_t width = WebPDemuxGetI(demux, WEBP_FF_CANVAS_WIDTH);
  uint32_t height = WebPDemuxGetI(demux, WEBP_FF_CANVAS_HEIGHT);

  const SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
  return new DecodingImageGenerator(info, data->data(), data->size());
}

DecodingImageGenerator::DecodingImageGenerator(const SkImageInfo info,
                                               const void* data,
                                               size_t size)
    : SkImageGenerator(info) {
  WebPDecoder(data, size, &decoded_bitmap_);
}

DecodingImageGenerator::~DecodingImageGenerator() {}

bool DecodingImageGenerator::onGetPixels(const SkImageInfo& info,
                                         void* pixels,
                                         size_t rowBytes,
                                         SkPMColor table[],
                                         int* tableCount) {
  SkAutoLockPixels bitmapLock(decoded_bitmap_);
  if (decoded_bitmap_.getPixels() != pixels) {
    return decoded_bitmap_.copyPixelsTo(pixels, rowBytes * info.height(),
                                        rowBytes);
  }
  return true;
}

bool DecodingImageGenerator::onQueryYUV8(SkYUVSizeInfo* sizeInfo,
                                         SkYUVColorSpace* colorSpace) const {
  return false;
}

bool DecodingImageGenerator::onGetYUV8Planes(const SkYUVSizeInfo& sizeInfo,
                                             void* planes[3]) {
  return false;
}

}  // namespace client
}  // namespace blimp
