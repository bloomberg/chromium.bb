// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_WALLPAPER_ARC_WALLPAPER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_WALLPAPER_ARC_WALLPAPER_SERVICE_H_

#include <stdint.h>
#include <vector>

#include "ash/common/wallpaper/wallpaper_controller_observer.h"
#include "base/macros.h"
#include "chrome/browser/image_decoder.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/wallpaper.mojom.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"

class SkBitmap;

namespace arc {

// Lives on the UI thread.
class ArcWallpaperService
    : public ArcService,
      public ash::WallpaperControllerObserver,
      public ImageDecoder::ImageRequest,
      public InstanceHolder<mojom::WallpaperInstance>::Observer,
      public mojom::WallpaperHost {
 public:
  explicit ArcWallpaperService(ArcBridgeService* bridge_service);
  ~ArcWallpaperService() override;

  // InstanceHolder<mojom::WallpaperInstance>::Observer overrides.
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

  // mojom::WallpaperHost overrides.
  // TODO(muyuanli): change callback prototype when use_new_wrapper_types is
  // updated and merge them with the functions below.
  void SetWallpaper(const std::vector<uint8_t>& data) override;
  void GetWallpaper(const GetWallpaperCallback& callback) override;

  // ImageDecoder::ImageRequest implementation.
  void OnImageDecoded(const SkBitmap& bitmap) override;
  void OnDecodeImageFailed() override;

  // WallpaperControllerObserver implementation.
  void OnWallpaperDataChanged() override;

 private:
  mojo::Binding<mojom::WallpaperHost> binding_;
  DISALLOW_COPY_AND_ASSIGN(ArcWallpaperService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_WALLPAPER_ARC_WALLPAPER_SERVICE_H_
