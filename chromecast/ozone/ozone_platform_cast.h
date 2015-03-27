// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_OZONE_OZONE_PLATFORM_CAST_H_
#define CHROMECAST_OZONE_OZONE_PLATFORM_CAST_H_

#include "chromecast/ozone/gpu_platform_support_cast.h"
#include "chromecast/ozone/surface_factory_cast.h"
#include "ui/ozone/public/ozone_platform.h"

namespace chromecast {
namespace ozone {

class CastEglPlatform;

// Ozone platform implementation for Cast.  Implements functionality
// common to all Cast implementations:
//  - Always one window with window size equal to display size
//  - No input, cursor support
//  - Relinquish GPU resources flow for switching to external applications
// Meanwhile, platform-specific implementation details are abstracted out
// to the CastEglPlatform interface.
class OzonePlatformCast : public ui::OzonePlatform {
 public:
  explicit OzonePlatformCast(scoped_ptr<CastEglPlatform> egl_platform);
  ~OzonePlatformCast() override;

  // OzonePlatform implementation:
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override;
  ui::CursorFactoryOzone* GetCursorFactoryOzone() override;
  ui::InputController* GetInputController() override;
  ui::GpuPlatformSupport* GetGpuPlatformSupport() override;
  ui::GpuPlatformSupportHost* GetGpuPlatformSupportHost() override;
  scoped_ptr<ui::SystemInputInjector> CreateSystemInputInjector() override;
  scoped_ptr<ui::PlatformWindow> CreatePlatformWindow(
      ui::PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) override;
  scoped_ptr<ui::NativeDisplayDelegate> CreateNativeDisplayDelegate() override;

  void InitializeUI() override;
  void InitializeGPU() override;

 private:
  scoped_ptr<CastEglPlatform> egl_platform_;
  scoped_ptr<SurfaceFactoryCast> surface_factory_ozone_;
  scoped_ptr<ui::CursorFactoryOzone> cursor_factory_ozone_;
  scoped_ptr<ui::InputController> input_controller_;
  scoped_ptr<GpuPlatformSupportCast> gpu_platform_support_;
  scoped_ptr<ui::GpuPlatformSupportHost> gpu_platform_support_host_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformCast);
};

}  // namespace ozone
}  // namespace chromecast

#endif  // CHROMECAST_OZONE_OZONE_PLATFORM_CAST_H_
