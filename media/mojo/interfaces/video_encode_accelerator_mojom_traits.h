// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_INTERFACES_VIDEO_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_
#define MEDIA_MOJO_INTERFACES_VIDEO_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_

#include "media/base/ipc/media_param_traits.h"
#include "media/mojo/interfaces/media_types.mojom.h"
#include "media/mojo/interfaces/video_encode_accelerator.mojom.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<media::mojom::VideoEncodeAccelerator::Error,
                  media::VideoEncodeAccelerator::Error> {
  static media::mojom::VideoEncodeAccelerator::Error ToMojom(
      media::VideoEncodeAccelerator::Error error);

  static bool FromMojom(media::mojom::VideoEncodeAccelerator::Error input,
                        media::VideoEncodeAccelerator::Error* out);
};

template <>
class StructTraits<media::mojom::VideoBitrateAllocationDataView,
                   media::VideoBitrateAllocation> {
 public:
  static std::vector<int32_t> bitrates(
      const media::VideoBitrateAllocation& bitrate_allocation);

  static bool Read(media::mojom::VideoBitrateAllocationDataView data,
                   media::VideoBitrateAllocation* out_bitrate_allocation);
};

template <>
class StructTraits<media::mojom::BitstreamBufferMetadataDataView,
                   media::BitstreamBufferMetadata> {
 public:
  static size_t payload_size_bytes(const media::BitstreamBufferMetadata& bbm) {
    return bbm.payload_size_bytes;
  }
  static bool key_frame(const media::BitstreamBufferMetadata& bbm) {
    return bbm.key_frame;
  }
  static base::TimeDelta timestamp(const media::BitstreamBufferMetadata& bbm) {
    return bbm.timestamp;
  }
  static const base::Optional<media::Vp8Metadata>& vp8(
      const media::BitstreamBufferMetadata& bbm) {
    return bbm.vp8;
  }

  static bool Read(media::mojom::BitstreamBufferMetadataDataView data,
                   media::BitstreamBufferMetadata* out_metadata);
};

template <>
class StructTraits<media::mojom::Vp8MetadataDataView, media::Vp8Metadata> {
 public:
  static bool non_reference(const media::Vp8Metadata& vp8) {
    return vp8.non_reference;
  }

  static uint8_t temporal_idx(const media::Vp8Metadata& vp8) {
    return vp8.temporal_idx;
  }

  static bool layer_sync(const media::Vp8Metadata& vp8) {
    return vp8.layer_sync;
  }

  static bool Read(media::mojom::Vp8MetadataDataView data,
                   media::Vp8Metadata* out_metadata);
};

template <>
struct StructTraits<media::mojom::VideoEncodeAcceleratorConfigDataView,
                    media::VideoEncodeAccelerator::Config> {
  static media::VideoPixelFormat input_format(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.input_format;
  }

  static const gfx::Size& input_visible_size(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.input_visible_size;
  }

  static media::VideoCodecProfile output_profile(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.output_profile;
  }

  static uint32_t initial_bitrate(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.initial_bitrate;
  }

  static uint8_t h264_output_level(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.h264_output_level.value_or(0);
  }

  static bool has_h264_output_level(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.h264_output_level.has_value();
  }

  static bool Read(media::mojom::VideoEncodeAcceleratorConfigDataView input,
                   media::VideoEncodeAccelerator::Config* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_INTERFACES_VIDEO_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_
