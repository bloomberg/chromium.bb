// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_wallpaper_instance.h"

namespace arc {

FakeWallpaperInstance::FakeWallpaperInstance() = default;

FakeWallpaperInstance::~FakeWallpaperInstance() = default;

void FakeWallpaperInstance::Init(mojom::WallpaperHostPtr host_ptr) {}

void FakeWallpaperInstance::OnWallpaperChanged(int32_t wallpaper_id) {
  changed_ids_.push_back(wallpaper_id);
}

}  // namespace arc
