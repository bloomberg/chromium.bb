// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ozone/ozone_platform_cast.h"

#include "chromecast/ozone/cast_egl_platform.h"
#include "chromecast/ozone/gpu_platform_support_cast.h"
#include "chromecast/ozone/platform_window_cast.h"
#include "chromecast/ozone/surface_factory_cast.h"
#include "ui/ozone/common/native_display_delegate_ozone.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/system_input_injector.h"

namespace chromecast {
namespace ozone {

OzonePlatformCast::OzonePlatformCast(scoped_ptr<CastEglPlatform> egl_platform)
    : egl_platform_(egl_platform.Pass()) {
}

OzonePlatformCast::~OzonePlatformCast() {
}

ui::SurfaceFactoryOzone* OzonePlatformCast::GetSurfaceFactoryOzone() {
  return surface_factory_ozone_.get();
}

ui::CursorFactoryOzone* OzonePlatformCast::GetCursorFactoryOzone() {
  return cursor_factory_ozone_.get();
}

ui::InputController* OzonePlatformCast::GetInputController() {
  return input_controller_.get();
}

ui::GpuPlatformSupport* OzonePlatformCast::GetGpuPlatformSupport() {
  return gpu_platform_support_.get();
}

ui::GpuPlatformSupportHost* OzonePlatformCast::GetGpuPlatformSupportHost() {
  return gpu_platform_support_host_.get();
}

scoped_ptr<ui::SystemInputInjector>
OzonePlatformCast::CreateSystemInputInjector() {
  return scoped_ptr<ui::SystemInputInjector>();  // no input injection support
}

scoped_ptr<ui::PlatformWindow> OzonePlatformCast::CreatePlatformWindow(
    ui::PlatformWindowDelegate* delegate,
    const gfx::Rect& bounds) {
  return make_scoped_ptr<ui::PlatformWindow>(
      new PlatformWindowCast(delegate, bounds));
}

scoped_ptr<ui::NativeDisplayDelegate>
OzonePlatformCast::CreateNativeDisplayDelegate() {
  return scoped_ptr<ui::NativeDisplayDelegate>(
      new ui::NativeDisplayDelegateOzone());
}

void OzonePlatformCast::InitializeUI() {
  if (!surface_factory_ozone_) {
    surface_factory_ozone_.reset(new SurfaceFactoryCast(egl_platform_.Pass()));
  }
  cursor_factory_ozone_.reset(new ui::CursorFactoryOzone());
  input_controller_ = ui::CreateStubInputController();
  gpu_platform_support_host_.reset(ui::CreateStubGpuPlatformSupportHost());
}

void OzonePlatformCast::InitializeGPU() {
  if (!surface_factory_ozone_) {
    surface_factory_ozone_.reset(new SurfaceFactoryCast(egl_platform_.Pass()));
  }
  gpu_platform_support_.reset(
      new GpuPlatformSupportCast(surface_factory_ozone_.get()));
}

}  // namespace ozone
}  // namespace chromecast
