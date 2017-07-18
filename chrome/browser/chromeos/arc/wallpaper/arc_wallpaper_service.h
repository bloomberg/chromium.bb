// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_WALLPAPER_ARC_WALLPAPER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_WALLPAPER_ARC_WALLPAPER_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "ash/wallpaper/wallpaper_controller_observer.h"
#include "base/macros.h"
#include "components/arc/common/wallpaper.mojom.h"
#include "components/arc/instance_holder.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Lives on the UI thread.
class ArcWallpaperService
    : public KeyedService,
      public ash::WallpaperControllerObserver,
      public InstanceHolder<mojom::WallpaperInstance>::Observer,
      public mojom::WallpaperHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcWallpaperService* GetForBrowserContext(
      content::BrowserContext* context);

  ArcWallpaperService(content::BrowserContext* context,
                      ArcBridgeService* bridge_service);
  ~ArcWallpaperService() override;

  // InstanceHolder<mojom::WallpaperInstance>::Observer overrides.
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

  // mojom::WallpaperHost overrides.
  // TODO(muyuanli): change callback prototype when use_new_wrapper_types is
  // updated and merge them with the functions below.
  void SetWallpaper(const std::vector<uint8_t>& data,
                    int32_t wallpaper_id) override;
  void SetDefaultWallpaper() override;
  void GetWallpaper(const GetWallpaperCallback& callback) override;

  // WallpaperControllerObserver implementation.
  void OnWallpaperDataChanged() override;

 private:
  class AndroidIdStore;
  class DecodeRequest;
  struct WallpaperIdPair;

  // Notifies wallpaper change if we have wallpaper instance.
  void NotifyWallpaperChanged(int android_id);
  // Notifies wallpaper change of |android_id|, then notify wallpaper change of
  // -1 to reset wallpaper cache at Android side.
  void NotifyWallpaperChangedAndReset(int android_id);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
  mojo::Binding<mojom::WallpaperHost> binding_;
  std::unique_ptr<DecodeRequest> decode_request_;
  std::vector<WallpaperIdPair> id_pairs_;

  DISALLOW_COPY_AND_ASSIGN(ArcWallpaperService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_WALLPAPER_ARC_WALLPAPER_SERVICE_H_
