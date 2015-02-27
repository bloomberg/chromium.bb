// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_READBACK_TYPES_H_
#define CONTENT_PUBLIC_BROWSER_READBACK_TYPES_H_

#include "base/callback.h"

class SkBitmap;

namespace content {

enum ReadbackResponse {
  READBACK_SUCCESS,
  READBACK_FAILED,
  READBACK_FORMAT_NOT_SUPPORTED,
  READBACK_NOT_SUPPORTED,
  READBACK_SURFACE_UNAVAILABLE,
  READBACK_MEMORY_ALLOCATION_FAILURE,
};

typedef const base::Callback<void(const SkBitmap&, ReadbackResponse)>
    ReadbackRequestCallback;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_READBACK_TYPES_H_
