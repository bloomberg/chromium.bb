// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ibus_controller.h"

#if defined(HAVE_IBUS)
#include "chrome/browser/chromeos/input_method/ibus_controller_impl.h"
#else
#include "chrome/browser/chromeos/input_method/mock_ibus_controller.h"
#endif

namespace chromeos {
namespace input_method {

IBusController::~IBusController() {
}

// static
IBusController* IBusController::Create() {
#if defined(HAVE_IBUS)
  return new IBusControllerImpl;
#else
  return new MockIBusController;
#endif
}

}  // namespace input_method
}  // namespace chromeos
