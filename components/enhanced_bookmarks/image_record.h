// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_IMAGE_RECORD_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_IMAGE_RECORD_H_

#include "base/memory/ref_counted.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace enhanced_bookmarks {

// Defines a record of a bookmark image in the ImageStore.
class ImageRecord :  public base::RefCountedThreadSafe<ImageRecord> {
 public:
  ImageRecord(scoped_ptr<gfx::Image> image,
              const GURL& url,
              SkColor dominant_color);
  ImageRecord(scoped_ptr<gfx::Image> image, const GURL& url);
  ImageRecord();

  // The image data.
  scoped_ptr<gfx::Image> image;
  // The URL that hosts the image.
  GURL url;
  // The dominant color of the image.
  SkColor dominant_color;

 private:
  friend class base::RefCountedThreadSafe<ImageRecord>;

  ~ImageRecord();
};

}  // namespace enhanced_bookmarks

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_IMAGE_RECORD_H_
