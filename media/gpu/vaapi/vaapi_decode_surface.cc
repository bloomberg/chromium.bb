// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_decode_surface.h"

namespace media {

VaapiDecodeSurface::VaapiDecodeSurface(
    int32_t bitstream_id,
    const scoped_refptr<VASurface>& va_surface)
    : bitstream_id_(bitstream_id), va_surface_(va_surface) {}

VaapiDecodeSurface::~VaapiDecodeSurface() = default;

}  // namespace media
