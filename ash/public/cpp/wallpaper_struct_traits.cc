// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper_struct_traits.h"

#include "ipc/ipc_message_utils.h"

namespace mojo {

bool StructTraits<ash::mojom::WallpaperInfoDataView, wallpaper::WallpaperInfo>::
    Read(ash::mojom::WallpaperInfoDataView data,
         wallpaper::WallpaperInfo* out) {
  return data.ReadLocation(&out->location) && data.ReadLayout(&out->layout) &&
         data.ReadType(&out->type) && data.ReadDate(&out->date);
}

}  // namespace mojo