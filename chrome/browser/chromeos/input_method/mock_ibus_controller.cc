// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/mock_ibus_controller.h"

#include "chromeos/ime/input_method_property.h"

namespace chromeos {
namespace input_method {

MockIBusController::MockIBusController()
    : activate_input_method_property_count_(0),
      activate_input_method_property_return_(true) {
}

MockIBusController::~MockIBusController() {
}

bool MockIBusController::ActivateInputMethodProperty(const std::string& key) {
  ++activate_input_method_property_count_;
  activate_input_method_property_key_ = key;
  return activate_input_method_property_return_;
}

}  // namespace input_method
}  // namespace chromeos
