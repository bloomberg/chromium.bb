// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/zucchini.h"

namespace zucchini {

status::Code Apply(ConstBufferView old_image,
                   const EnsemblePatchReader& patch_reader,
                   MutableBufferView new_image) {
  if (!patch_reader.CheckOldFile(old_image)) {
    LOG(ERROR) << "Invalid old_image.";
    return zucchini::status::kStatusInvalidOldImage;
  }

  // TODO(etiennep): Implement.

  // This will always fail for now, because of missing implementation.
  if (!patch_reader.CheckNewFile(ConstBufferView(new_image))) {
    LOG(ERROR) << "Invalid new_image.";
    return zucchini::status::kStatusInvalidNewImage;
  }
  return zucchini::status::kStatusSuccess;
}

}  // namespace zucchini
