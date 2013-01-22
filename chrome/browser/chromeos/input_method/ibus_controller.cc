// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ibus_controller.h"

#include "base/chromeos/chromeos_version.h"
#include "chrome/browser/chromeos/input_method/ibus_controller_impl.h"
#include "chrome/browser/chromeos/input_method/mock_ibus_controller.h"

namespace chromeos {
namespace input_method {

IBusController::~IBusController() {
}

// static
IBusController* IBusController::Create() {
  if (base::chromeos::IsRunningOnChromeOS())
    return new IBusControllerImpl;
  else
    return new MockIBusController;
}

}  // namespace input_method
}  // namespace chromeos
