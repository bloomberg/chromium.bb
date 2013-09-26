// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/mock_ime_property_handler.h"

namespace chromeos {

MockIMEPropertyHandler::MockIMEPropertyHandler()
    : register_properties_call_count_(0) {
}

MockIMEPropertyHandler::~MockIMEPropertyHandler() {
}

void MockIMEPropertyHandler::RegisterProperties(
  const input_method::InputMethodPropertyList& properties) {
  ++register_properties_call_count_;
  last_registered_properties_ = properties;
}

void MockIMEPropertyHandler::Reset() {
  register_properties_call_count_ = 0;
}

}  // namespace chromeos
