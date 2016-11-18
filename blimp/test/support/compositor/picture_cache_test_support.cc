// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/test/support/compositor/picture_cache_test_support.h"

#include "base/memory/ptr_util.h"
#include "blimp/common/blob_cache/blob_cache.h"
#include "blimp/common/blob_cache/id_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/skia_util.h"

namespace blimp {

sk_sp<const SkPicture> CreateSkPicture(SkColor color) {
  SkPictureRecorder recorder;
  sk_sp<SkCanvas> canvas =
      sk_ref_sp(recorder.beginRecording(SkRect::MakeWH(1, 1)));
  canvas->drawColor(color);
  return recorder.finishRecordingAsPicture();
}

sk_sp<SkData> SerializePicture(sk_sp<const SkPicture> picture) {
  sk_sp<SkData> data = picture->serialize();
  DCHECK_GT(data->size(), 0u);
  return data;
}

BlobId GetBlobId(sk_sp<const SkPicture> picture) {
  sk_sp<SkData> data = SerializePicture(picture);
  return CalculateBlobId(data->data(), data->size());
}

sk_sp<const SkPicture> DeserializePicture(sk_sp<SkData> data) {
  return SkPicture::MakeFromData(data.get());
}

bool PicturesEqual(sk_sp<const SkPicture> picture,
                   const cc::PictureData& picture_data) {
  if (picture->uniqueID() != picture_data.unique_id) {
    return false;
  }
  sk_sp<const SkPicture> deserialized_picture =
      DeserializePicture(picture_data.data);
  return GetBlobId(picture) == GetBlobId(deserialized_picture);
}

cc::PictureData CreatePictureData(sk_sp<const SkPicture> picture) {
  return cc::PictureData(picture->uniqueID(), SerializePicture(picture));
}

}  // namespace blimp
