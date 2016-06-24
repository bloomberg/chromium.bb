// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_TEST_SUPPORT_COMPOSITOR_PICTURE_CACHE_TEST_SUPPORT_H_
#define BLIMP_TEST_SUPPORT_COMPOSITOR_PICTURE_CACHE_TEST_SUPPORT_H_

#include "blimp/common/blob_cache/blob_cache.h"
#include "cc/blimp/picture_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blimp {

// Creates an SkPicture filled with with the given |color|.
sk_sp<const SkPicture> CreateSkPicture(SkColor color);

// Serializes the given picture into an SkData object.
sk_sp<SkData> SerializePicture(sk_sp<const SkPicture> picture);

// Serializes an SkPicture and returns the BlobId for the serialized form of
// the picture.
BlobId GetBlobId(sk_sp<const SkPicture> picture);

// Deserializes the SkPicture represented by |data.
sk_sp<const SkPicture> DeserializePicture(sk_sp<SkData> data);

// Compares an SkPicture and SkPicture represented by the SkData in the
// cc::PictureData.
bool PicturesEqual(sk_sp<const SkPicture> picture,
                   const cc::PictureData& picture_data);

// Utility function to create a valid cc::PictureData from an SkPicture.
cc::PictureData CreatePictureData(sk_sp<const SkPicture> picture);

MATCHER_P(PictureMatches, picture, "") {
  return PicturesEqual(picture, arg);
}

}  // namespace blimp

#endif  // BLIMP_TEST_SUPPORT_COMPOSITOR_PICTURE_CACHE_TEST_SUPPORT_H_
