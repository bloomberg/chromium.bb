// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_profile.h"

#include <string>

namespace device {

BluetoothProfile::Options::Options()
    : channel(0),
      psm(0),
      require_authentication(false),
      require_authorization(false),
      version(0),
      features(0) {
}

BluetoothProfile::Options::~Options() {

}


BluetoothProfile::BluetoothProfile() {

}

BluetoothProfile::~BluetoothProfile() {

}


// static
void BluetoothProfile::Register(const std::string& uuid,
                                const Options& options,
                                const ProfileCallback& callback) {
  // TODO(keybuk): Implement selection of the appropriate BluetoothProfile
  // subclass just like BluetoothAdapterFactory
  callback.Run(NULL);
}

}  // namespace device
