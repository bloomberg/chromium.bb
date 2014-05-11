// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_IMAGE_STORE_UTIL_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_IMAGE_STORE_UTIL_H_

#include "base/memory/ref_counted_memory.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

namespace enhanced_bookmarks {
// The two methods below archive and unarchive an image to and from a bag of
// bytes. There is no API on gfx::Image capable of doing it while preserving the
// scale of the image.

// Encode |image| to bytes, that can be decoded using the below |ImageForBytes|
// function. If encoding fails, then returned scoped_refptr has NULL.
scoped_refptr<base::RefCountedMemory> BytesForImage(const gfx::Image& image);

// Decode the image bytes encoded by the above |BytesForImage| function and
// returns a gfx::Image. If decoding fails, returns an empty image.
gfx::Image ImageForBytes(const scoped_refptr<base::RefCountedMemory>& data);

}

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_IMAGE_STORE_UTIL_H_
