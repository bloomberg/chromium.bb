// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/image_record.h"

#include <utility>

namespace enhanced_bookmarks {

ImageRecord::ImageRecord(scoped_ptr<gfx::Image> image,
                         const GURL& url,
                         SkColor dominant_color)
    : image(std::move(image)), url(url), dominant_color(dominant_color) {}

ImageRecord::ImageRecord(scoped_ptr<gfx::Image> image, const GURL& url)
    : ImageRecord(std::move(image), url, SK_ColorBLACK) {}

ImageRecord::ImageRecord()
    : ImageRecord(scoped_ptr<gfx::Image>(new gfx::Image()), GURL()) {
}

ImageRecord::~ImageRecord() {
}

}  // namespace enhanced_bookmarks
