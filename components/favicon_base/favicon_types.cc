// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/favicon_types.h"

namespace favicon_base {

// FaviconImageResult ---------------------------------------------------------

FaviconImageResult::FaviconImageResult() {}

FaviconImageResult::~FaviconImageResult() {}

// FaviconRawBitmapResult
// --------------------------------------------------------

FaviconRawBitmapResult::FaviconRawBitmapResult()
    : expired(false), icon_type(INVALID_ICON) {
}

FaviconRawBitmapResult::~FaviconRawBitmapResult() {
}

}  // namespace chrome
