// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_DISPLAY_CONFIGURATOR_H_
#define CHROMECAST_BROWSER_CAST_DISPLAY_CONFIGURATOR_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/display/types/native_display_observer.h"

namespace display {
class DisplaySnapshot;
class NativeDisplayDelegate;
}  // namespace display

namespace gfx {
class Rect;
}  // namespace gfx

namespace chromecast {
class CastScreen;

namespace shell {

// The CastDisplayConfigurator class ensures native displays are initialized and
// configured properly on platforms that need that (e.g. GBM/DRM graphics via
// OzonePlatformGbm on odroid). But OzonePlatformCast, used by most Cast
// devices, relies on the platform code (outside of cast_shell) to initialize
// displays and exposes only a FakeDisplayDelegate. So CastDisplayConfigurator
// doesn't really do anything when using OzonePlatformCast.
class CastDisplayConfigurator : public display::NativeDisplayObserver {
 public:
  explicit CastDisplayConfigurator(CastScreen* screen);
  ~CastDisplayConfigurator() override;

  // display::NativeDisplayObserver implementation
  void OnConfigurationChanged() override;
  void OnDisplaySnapshotsInvalidated() override {}

 private:
  void OnDisplaysAcquired(
      const std::vector<display::DisplaySnapshot*>& displays);
  void OnDisplayConfigured(const gfx::Rect& bounds, bool success);

  std::unique_ptr<display::NativeDisplayDelegate> delegate_;
  CastScreen* const cast_screen_;

  base::WeakPtrFactory<CastDisplayConfigurator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastDisplayConfigurator);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_DISPLAY_CONFIGURATOR_H_
