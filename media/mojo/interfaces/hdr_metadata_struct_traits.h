// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_INTERFACES_HDR_METADATA_STRUCT_TRAITS_H_
#define MEDIA_MOJO_INTERFACES_HDR_METADATA_STRUCT_TRAITS_H_

#include "media/base/hdr_metadata.h"
#include "media/mojo/interfaces/media_types.mojom.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::MasteringMetadataDataView,
                    media::MasteringMetadata> {
  static float primary_r_chromaticity_x(const media::MasteringMetadata& input) {
    return input.primary_r_chromaticity_x;
  }
  static float primary_r_chromaticity_y(const media::MasteringMetadata& input) {
    return input.primary_r_chromaticity_y;
  }
  static float primary_g_chromaticity_x(const media::MasteringMetadata& input) {
    return input.primary_g_chromaticity_x;
  }
  static float primary_g_chromaticity_y(const media::MasteringMetadata& input) {
    return input.primary_g_chromaticity_y;
  }
  static float primary_b_chromaticity_x(const media::MasteringMetadata& input) {
    return input.primary_b_chromaticity_x;
  }
  static float primary_b_chromaticity_y(const media::MasteringMetadata& input) {
    return input.primary_b_chromaticity_y;
  }
  static float white_point_chromaticity_x(const media::MasteringMetadata& inp) {
    return inp.white_point_chromaticity_x;
  }
  static float white_point_chromaticity_y(const media::MasteringMetadata& inp) {
    return inp.white_point_chromaticity_y;
  }
  static float luminance_max(const media::MasteringMetadata& input) {
    return input.luminance_max;
  }
  static float luminance_min(const media::MasteringMetadata& input) {
    return input.luminance_min;
  }

  static bool Read(media::mojom::MasteringMetadataDataView data,
                   media::MasteringMetadata* output) {
    output->primary_r_chromaticity_x = data.primary_r_chromaticity_x();
    output->primary_r_chromaticity_y = data.primary_r_chromaticity_y();
    output->primary_g_chromaticity_x = data.primary_g_chromaticity_x();
    output->primary_g_chromaticity_y = data.primary_g_chromaticity_y();
    output->primary_b_chromaticity_x = data.primary_b_chromaticity_x();
    output->primary_b_chromaticity_y = data.primary_b_chromaticity_y();
    output->white_point_chromaticity_x = data.white_point_chromaticity_x();
    output->white_point_chromaticity_y = data.white_point_chromaticity_y();
    output->luminance_max = data.luminance_max();
    output->luminance_min = data.luminance_min();
    return true;
  }
};

template <>
struct StructTraits<media::mojom::HDRMetadataDataView, media::HDRMetadata> {
  static unsigned max_cll(const media::HDRMetadata& input) {
    return input.max_cll;
  }
  static unsigned max_fall(const media::HDRMetadata& input) {
    return input.max_fall;
  }
  static media::MasteringMetadata mastering_metadata(
      const media::HDRMetadata& input) {
    return input.mastering_metadata;
  }

  static bool Read(media::mojom::HDRMetadataDataView data,
                   media::HDRMetadata* output) {
    output->max_cll = data.max_cll();
    output->max_fall = data.max_fall();
    return true;
  }
};

}  // namespace mojo

#endif  // MEDIA_MOJO_INTERFACES_HDR_METADATA_STRUCT_TRAITS_H_
