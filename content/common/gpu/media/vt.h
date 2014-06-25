// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_VT_H_
#define CONTENT_COMMON_GPU_MEDIA_VT_H_

// Dynamic library loader.
#include "content/common/gpu/media/vt_stubs.h"

// CoreMedia and VideoToolbox types.
#include "content/common/gpu/media/vt_stubs_header.fragment"

// CoreMedia and VideoToolbox functions.
extern "C" {
#include "content/common/gpu/media/vt.sig"
}  // extern "C"

#endif  // CONTENT_COMMON_GPU_MEDIA_VT_H_
