// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_RESOURCES_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_RESOURCES_H_

class SkBitmap;

namespace ash {

int GetDefaultWallpaperIndex();
int GetWallpaperCount();
const SkBitmap& GetWallpaper(int index);
const SkBitmap& GetWallpaperThumbnail(int index);

}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_RESOURCES_H_
