// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_display_configurator.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/browser/cast_touch_device_manager.h"
#include "chromecast/graphics/cast_screen.h"
#include "chromecast/public/graphics_properties_shlib.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/public/ozone_platform.h"

namespace chromecast {
namespace shell {

namespace {
constexpr int64_t kStubDisplayId = 1;
constexpr char kCastGraphicsHeight[] = "cast-graphics-height";
constexpr char kCastGraphicsWidth[] = "cast-graphics-width";
constexpr char kDisplayRotation[] = "display-rotation";

// Helper to return the screen resolution (device pixels)
// to use.
gfx::Size GetScreenResolution() {
  gfx::Size res(1280, 720);
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!base::FeatureList::IsEnabled(kTripleBuffer720) &&
      GraphicsPropertiesShlib::IsSupported(GraphicsPropertiesShlib::k1080p,
                                           cmd_line->argv())) {
    res = gfx::Size(1920, 1080);
  }

  int cast_gfx_width = 0;
  int cast_gfx_height = 0;
  if (base::StringToInt(cmd_line->GetSwitchValueASCII(kCastGraphicsWidth),
                        &cast_gfx_width) &&
      base::StringToInt(cmd_line->GetSwitchValueASCII(kCastGraphicsHeight),
                        &cast_gfx_height) &&
      cast_gfx_width > 0 && cast_gfx_height > 0) {
    res.set_width(cast_gfx_width);
    res.set_height(cast_gfx_height);
  }
  return res;
}

display::Display::Rotation GetRotationFromCommandLine() {
  std::string rotation =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          kDisplayRotation);
  if (rotation == "90")
    return display::Display::ROTATE_90;
  else if (rotation == "180")
    return display::Display::ROTATE_180;
  else if (rotation == "270")
    return display::Display::ROTATE_270;
  else
    return display::Display::ROTATE_0;
}

float GetDeviceScaleFactor(gfx::Size display_resolution) {
  // Device scale factor computed relative to 720p display
  return display_resolution.height() / 720.0f;
}

}  // namespace

CastDisplayConfigurator::CastDisplayConfigurator(CastScreen* screen)
    : delegate_(
          ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate()),
      touch_device_manager_(std::make_unique<CastTouchDeviceManager>()),
      cast_screen_(screen),
      weak_factory_(this) {
  if (delegate_) {
    delegate_->AddObserver(this);
    delegate_->Initialize();
    OnConfigurationChanged();
  } else {
    ConfigureDisplayFromCommandLine();
  }
}

CastDisplayConfigurator::~CastDisplayConfigurator() {
  if (delegate_)
    delegate_->RemoveObserver(this);
}

// display::NativeDisplayObserver interface
void CastDisplayConfigurator::OnConfigurationChanged() {
  DCHECK(delegate_);
  delegate_->GetDisplays(
      base::Bind(&CastDisplayConfigurator::OnDisplaysAcquired,
                 weak_factory_.GetWeakPtr()));
}

void CastDisplayConfigurator::ConfigureDisplayFromCommandLine() {
  const gfx::Size size = GetScreenResolution();
  UpdateScreen(kStubDisplayId, gfx::Rect(size), GetDeviceScaleFactor(size),
               GetRotationFromCommandLine());
}

void CastDisplayConfigurator::OnDisplaysAcquired(
    const std::vector<display::DisplaySnapshot*>& displays) {
  DCHECK(delegate_);
  if (displays.empty()) {
    LOG(WARNING) << "No displays detected, skipping display init.";
    return;
  }

  if (displays.size() > 1) {
    LOG(WARNING) << "Multiple display detected, using the first one.";
  }

  auto* display = displays[0];
  if (!display->native_mode()) {
    LOG(WARNING) << "Display " << display->display_id()
                 << " doesn't have a native mode.";
    return;
  }

  gfx::Point origin;
  delegate_->Configure(
      *display, display->native_mode(), origin,
      base::BindRepeating(&CastDisplayConfigurator::OnDisplayConfigured,
                          weak_factory_.GetWeakPtr(), display,
                          display->native_mode(), origin));
}

void CastDisplayConfigurator::OnDisplayConfigured(
    display::DisplaySnapshot* display,
    const display::DisplayMode* mode,
    const gfx::Point& origin,
    bool success) {
  DCHECK(display);
  DCHECK(mode);

  const gfx::Rect bounds(origin, mode->size());
  VLOG(1) << __func__ << " success=" << success
          << " bounds=" << bounds.ToString();
  if (success) {
    // Need to update the display state otherwise it becomes stale.
    display->set_current_mode(mode);
    display->set_origin(origin);

    UpdateScreen(display->display_id(), bounds,
                 GetDeviceScaleFactor(display->native_mode()->size()),
                 GetRotationFromCommandLine());
  } else {
    LOG(FATAL) << "Failed to configure display";
  }
}

void CastDisplayConfigurator::UpdateScreen(
    int64_t display_id,
    const gfx::Rect& bounds,
    float device_scale_factor,
    display::Display::Rotation rotation) {
  cast_screen_->OnDisplayChanged(display_id, device_scale_factor, rotation,
                                 bounds);
  touch_device_manager_->OnDisplayConfigured(display_id, rotation, bounds);
}

}  // namespace shell
}  // namespace chromecast
