// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_image_decoder.h"

#include <va/va.h>

#include "media/gpu/vaapi/vaapi_wrapper.h"

namespace media {

VaapiImageDecoder::VaapiImageDecoder()
    : va_surface_id_(VA_INVALID_SURFACE), va_rt_format_(kInvalidVaRtFormat) {}

VaapiImageDecoder::~VaapiImageDecoder() = default;

}  // namespace media
