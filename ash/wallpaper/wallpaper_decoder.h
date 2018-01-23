// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_DECODER_H_
#define ASH_WALLPAPER_WALLPAPER_DECODER_H_

#include "ash/ash_export.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"

namespace gfx {
class ImageSkia;
}

namespace ash {

using OnWallpaperDecoded =
    base::OnceCallback<void(const gfx::ImageSkia& image)>;

// Do an async wallpaper decode; |on_decoded| is run on the calling thread when
// the decode has finished.
ASH_EXPORT void DecodeWallpaper(const std::string& image_data,
                                OnWallpaperDecoded callback);

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_DECODER_H_