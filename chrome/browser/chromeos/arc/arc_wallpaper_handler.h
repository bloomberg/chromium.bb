// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_WALLPAPER_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_WALLPAPER_HANDLER_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/image_decoder.h"
#include "components/arc/set_wallpaper_delegate.h"

class SkBitmap;

namespace arc {

class ArcWallpaperHandler : public SetWallpaperDelegate {
 public:
  ArcWallpaperHandler();
  ~ArcWallpaperHandler() override;

  // SetWallpaperDelegate implementation.
  void SetWallpaper(const std::vector<uint8_t>& jpeg_data) override;

 private:
  class ImageRequestImpl;

  // Called from ImageRequestImpl on decode completion.
  void OnImageDecoded(ImageRequestImpl* request, const SkBitmap& bitmap);
  void OnDecodeImageFailed(ImageRequestImpl* request);

  base::ThreadChecker thread_checker_;

  // The set of in-flight decode requests. Pointers are owned.
  std::set<ImageRequestImpl*> inflight_requests_;

  DISALLOW_COPY_AND_ASSIGN(ArcWallpaperHandler);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_WALLPAPER_HANDLER_H_
