// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_profile.h"

#if defined(OS_CHROMEOS)
#include "device/bluetooth/bluetooth_profile_chromeos.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "device/bluetooth/bluetooth_profile_mac.h"
#elif defined(OS_WIN)
#include "device/bluetooth/bluetooth_profile_win.h"
#endif

#include <string>

namespace device {

BluetoothProfile::Options::Options()
    : channel(0),
      psm(0),
      require_authentication(false),
      require_authorization(false),
      auto_connect(false),
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
#if defined(OS_CHROMEOS)
  chromeos::BluetoothProfileChromeOS* profile = NULL;
  profile = new chromeos::BluetoothProfileChromeOS();
  profile->Init(uuid, options, callback);
#elif defined(OS_MACOSX)
  BluetoothProfile* profile = NULL;

  if (base::mac::IsOSLionOrLater())
    profile = new BluetoothProfileMac(uuid, options.name);
  callback.Run(profile);
#elif defined(OS_WIN)
  BluetoothProfile* profile = NULL;
  profile = new BluetoothProfileWin(uuid, options.name);
  callback.Run(profile);
#else
  callback.Run(NULL);
#endif
}

}  // namespace device
