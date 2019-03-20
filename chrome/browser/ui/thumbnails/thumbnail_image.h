// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_IMAGE_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_IMAGE_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/image/image_skia.h"

// Value type holding thumbnail image data. Data is internally refcounted, so
// copying this object is inexpensive.
//
// Internally, data is stored as a ThumbnailRepresentation, which is optionally
// compressed and not directly renderable, so that holding a thumbnail from each
// tab in memory isn't prohibitively expensive. Because of this, and because
// converting from the uncompressed to the compressed format may be expensive,
// asynchronous methods for converting to and from ThumbnailImage are provided.
class ThumbnailImage {
 public:
  // Callback used by AsImageSkiaAsync. The parameter is the retrieved image.
  using AsImageSkiaCallback = base::OnceCallback<void(gfx::ImageSkia)>;

  // Callback used by FromSkBitmapAsync. The parameter is the created thumbnail.
  using CreateThumbnailCallback = base::OnceCallback<void(ThumbnailImage)>;

  ThumbnailImage();
  ~ThumbnailImage();
  ThumbnailImage(const ThumbnailImage& other);
  ThumbnailImage(ThumbnailImage&& other);
  ThumbnailImage& operator=(const ThumbnailImage& other);
  ThumbnailImage& operator=(ThumbnailImage&& other);

  // Retrieves the thumbnail data as a renderable image. May involve image
  // decoding, so could be expensive. Prefer AsImageSkiaAsync() instead.
  gfx::ImageSkia AsImageSkia() const;

  // Retrieves the thumbnail data as a renderable image. May involve image
  // decoding on a background thread. Callback is called when the operation
  // completes. Returns false if there is no thumbnail data.
  bool AsImageSkiaAsync(AsImageSkiaCallback callback) const;

  // Returns whether there is data stored in the thumbnail.
  bool HasData() const;

  // Returns the size (in bytes) required to store the internal representation.
  size_t GetStorageSize() const;

  // Does an address comparison on the refcounted backing store of this object.
  // May return false even if the backing stores contain equivalent image data.
  bool BackedBySameObjectAs(const ThumbnailImage& other) const;

  // Encodes thumbnail data as ThumbnailRepresentation. May involve an expensive
  // operation. Prefer FromSkBitmapAsync() instead.
  static ThumbnailImage FromSkBitmap(SkBitmap bitmap);

  // Converts the bitmap data to the internal ThumbnailRepresentation. May
  // involve image encoding or compression on a background thread. Callback is
  // called when the operation completes.
  static void FromSkBitmapAsync(SkBitmap bitmap,
                                CreateThumbnailCallback callback);

 private:
  class ThumbnailData;

  scoped_refptr<ThumbnailData> image_representation_;
};

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_IMAGE_H_
