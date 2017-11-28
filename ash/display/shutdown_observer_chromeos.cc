// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/shutdown_observer_chromeos.h"

#include "ui/display/manager/chromeos/display_configurator.h"

namespace ash {

ShutdownObserver::ShutdownObserver(
    display::DisplayConfigurator* display_configurator)
    : display_configurator_(display_configurator),
      scoped_session_observer_(this) {}

ShutdownObserver::~ShutdownObserver() = default;

void ShutdownObserver::OnChromeTerminating() {
  // Stop handling display configuration events once the shutdown
  // process starts. crbug.com/177014.
  display_configurator_->PrepareForExit();
}

}  // namespace ash
