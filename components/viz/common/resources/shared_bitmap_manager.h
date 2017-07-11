// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_BITMAP_MANAGER_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_BITMAP_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "components/viz/common/quads/shared_bitmap.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

class SharedBitmapManager {
 public:
  SharedBitmapManager() {}
  virtual ~SharedBitmapManager() {}

  virtual std::unique_ptr<SharedBitmap> AllocateSharedBitmap(
      const gfx::Size&) = 0;
  virtual std::unique_ptr<SharedBitmap> GetSharedBitmapFromId(
      const gfx::Size&,
      const SharedBitmapId&) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedBitmapManager);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_BITMAP_MANAGER_H_
