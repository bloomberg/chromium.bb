// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PICTURE_CACHE_MODEL_H_
#define CC_TEST_PICTURE_CACHE_MODEL_H_

#include <stdint.h>

#include <unordered_map>

#include "base/macros.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkPicture;

namespace cc {

// PictureCacheModel provides a way for sharing a cache between an engine
// and the client. When the FakeEnginePictureCache and the ClientPictureCache
// point to the same PictureCacheModel, the calls to AddPicture on the
// engine make an SkPicture available through GetPicture on the client.
class PictureCacheModel {
 public:
  PictureCacheModel();
  ~PictureCacheModel();

  void AddPicture(const SkPicture* picture);

  sk_sp<const SkPicture> GetPicture(uint32_t unique_id);

 private:
  std::unordered_map<uint32_t, sk_sp<const SkPicture>> pictures_;

  DISALLOW_COPY_AND_ASSIGN(PictureCacheModel);
};

}  // namespace cc

#endif  // CC_TEST_PICTURE_CACHE_MODEL_H_
