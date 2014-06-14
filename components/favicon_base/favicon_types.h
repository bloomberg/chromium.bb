// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_BASE_FAVICON_TYPES_H_
#define COMPONENTS_FAVICON_BASE_FAVICON_TYPES_H_

#include "base/memory/ref_counted_memory.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"

namespace favicon_base {

typedef int64 FaviconID;

// Defines the icon types. They are also stored in icon_type field of favicons
// table.
// The values of the IconTypes are used to select the priority in which favicon
// data is returned in HistoryBackend and ThumbnailDatabase. Data for the
// largest IconType takes priority if data for multiple IconTypes is available.
enum IconType {
  INVALID_ICON = 0x0,
  FAVICON = 1 << 0,
  TOUCH_ICON = 1 << 1,
  TOUCH_PRECOMPOSED_ICON = 1 << 2
};

// Defines a gfx::Image of size desired_size_in_dip composed of image
// representations for each of the desired scale factors.
struct FaviconImageResult {
  FaviconImageResult();
  ~FaviconImageResult();

  // The resulting image.
  gfx::Image image;

  // The URL of the favicon which contains all of the image representations of
  // |image|.
  // TODO(pkotwicz): Return multiple |icon_urls| to allow |image| to have
  // representations from several favicons once content::FaviconStatus supports
  // multiple URLs.
  GURL icon_url;
};

// Defines a favicon bitmap which best matches the desired DIP size and one of
// the desired scale factors.
struct FaviconRawBitmapResult {
  FaviconRawBitmapResult();
  ~FaviconRawBitmapResult();

  // Returns true if |bitmap_data| contains a valid bitmap.
  bool is_valid() const { return bitmap_data.get() && bitmap_data->size(); }

  // Indicates whether |bitmap_data| is expired.
  bool expired;

  // The bits of the bitmap.
  scoped_refptr<base::RefCountedMemory> bitmap_data;

  // The pixel dimensions of |bitmap_data|.
  gfx::Size pixel_size;

  // The URL of the containing favicon.
  GURL icon_url;

  // The icon type of the containing favicon.
  IconType icon_type;
};

// Define type with same structure as FaviconRawBitmapResult for passing data to
// HistoryBackend::SetFavicons().
typedef FaviconRawBitmapResult FaviconRawBitmapData;

}  // namespace favicon_base

#endif  // COMPONENTS_FAVICON_BASE_FAVICON_TYPES_H_
