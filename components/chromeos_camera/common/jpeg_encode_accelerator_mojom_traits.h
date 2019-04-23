// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHROMEOS_CAMERA_COMMON_JPEG_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_
#define COMPONENTS_CHROMEOS_CAMERA_COMMON_JPEG_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_

#include "components/chromeos_camera/common/jpeg_encode_accelerator.mojom.h"
#include "media/video/jpeg_encode_accelerator.h"

namespace mojo {

template <>
struct EnumTraits<chromeos_camera::mojom::EncodeStatus,
                  media::JpegEncodeAccelerator::Status> {
  static chromeos_camera::mojom::EncodeStatus ToMojom(
      media::JpegEncodeAccelerator::Status status);

  static bool FromMojom(chromeos_camera::mojom::EncodeStatus input,
                        media::JpegEncodeAccelerator::Status* out);
};

}  // namespace mojo

#endif  // COMPONENTS_CHROMEOS_CAMERA_COMMON_JPEG_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_
