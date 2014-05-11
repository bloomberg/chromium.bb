// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/image_store_util.h"

#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_util.h"

namespace {
const int kJpegEncodingQuality = 70;
}  // namespace

namespace enhanced_bookmarks {

scoped_refptr<base::RefCountedMemory> BytesForImage(const gfx::Image& image) {
  DCHECK(image.AsImageSkia().image_reps().size() == 1);
  DCHECK(image.AsImageSkia().image_reps().begin()->scale() == 1.0f);

  std::vector<unsigned char> data;
  bool succeeded =
      gfx::JPEG1xEncodedDataFromImage(image, kJpegEncodingQuality, &data);

  if (!succeeded)
    return scoped_refptr<base::RefCountedMemory>();

  return scoped_refptr<base::RefCountedMemory>(new base::RefCountedBytes(data));
}

gfx::Image ImageForBytes(const scoped_refptr<base::RefCountedMemory>& bytes) {
  return gfx::ImageFrom1xJPEGEncodedData(bytes->front(), bytes->size());
}
}
