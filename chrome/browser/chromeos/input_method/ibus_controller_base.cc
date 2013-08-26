// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ibus_controller_base.h"

namespace chromeos {
namespace input_method {

IBusControllerBase::IBusControllerBase() {
}

IBusControllerBase::~IBusControllerBase() {
}

void IBusControllerBase::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void IBusControllerBase::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const InputMethodPropertyList&
IBusControllerBase::GetCurrentProperties() const {
  return current_property_list_;
}

void IBusControllerBase::ClearProperties() {
  current_property_list_.clear();
}

}  // namespace input_method
}  // namespace chromeos
