// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/favicon/favicon_types.h"

namespace chrome {

// FaviconImageResult ---------------------------------------------------------

FaviconImageResult::FaviconImageResult() {}

FaviconImageResult::~FaviconImageResult() {}

// FaviconBitmapResult --------------------------------------------------------

FaviconBitmapResult::FaviconBitmapResult()
    : expired(false),
      icon_type(INVALID_ICON) {}

FaviconBitmapResult::~FaviconBitmapResult() {}

}  // namespace chrome
