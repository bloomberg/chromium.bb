// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_display_configurator.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chromecast/graphics/cast_screen.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/public/ozone_platform.h"

namespace chromecast {
namespace shell {

CastDisplayConfigurator::CastDisplayConfigurator(CastScreen* screen)
    : delegate_(
          ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate()),
      cast_screen_(screen),
      weak_factory_(this) {
  if (!delegate_)
    return;
  DCHECK(cast_screen_);
  delegate_->AddObserver(this);
  delegate_->Initialize();
  OnConfigurationChanged();
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

    cast_screen_->OnDisplayChanged(1.0f, bounds);
  } else {
    LOG(FATAL) << "Failed to configure display";
  }
}

}  // namespace shell
}  // namespace chromecast
