// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_THIRD_PARTY_VPN_OBSERVER_H_
#define CHROMEOS_DBUS_SHILL_THIRD_PARTY_VPN_OBSERVER_H_

#include <stdint.h>

namespace chromeos {

// This is a base class for observers which handle signals sent by the
// ThirdPartyVpnAdaptor in Shill.
class ShillThirdPartyVpnObserver {
 public:
  // Ownership of |data| belongs to the caller, hence the contents should be
  // consumed before the call returns, i.e., pointer should not be dereferenced
  // after the function returns. The method takes raw data pointer and length
  // instead of a type like vector to avoid additional memcpys.
  virtual void OnPacketReceived(const uint8_t* data, size_t length) = 0;
  virtual void OnPlatformMessage(uint32_t message) = 0;

 protected:
  virtual ~ShillThirdPartyVpnObserver() {}

 private:
  DISALLOW_ASSIGN(ShillThirdPartyVpnObserver);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_THIRD_PARTY_VPN_OBSERVER_H_
