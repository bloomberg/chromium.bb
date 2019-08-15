// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_CABLE_DISCOVERY_DATA_H_
#define DEVICE_FIDO_CABLE_CABLE_DISCOVERY_DATA_H_

#include <stdint.h>
#include <array>

#include "base/component_export.h"

namespace device {

constexpr size_t kCableEphemeralIdSize = 16;
constexpr size_t kCableSessionPreKeySize = 32;

using CableEidArray = std::array<uint8_t, kCableEphemeralIdSize>;
using CableSessionPreKeyArray = std::array<uint8_t, kCableSessionPreKeySize>;

// Encapsulates information required to discover Cable device per single
// credential. When multiple credentials are enrolled to a single account
// (i.e. more than one phone has been enrolled to an user account as a
// security key), then FidoCableDiscovery must advertise for all of the client
// EID received from the relying party.
// TODO(hongjunchoi): Add discovery data required for MakeCredential request.
// See: https://crbug.com/837088
struct COMPONENT_EXPORT(DEVICE_FIDO) CableDiscoveryData {
  CableDiscoveryData(uint8_t version,
                     const CableEidArray& client_eid,
                     const CableEidArray& authenticator_eid,
                     const CableSessionPreKeyArray& session_pre_key);
  CableDiscoveryData();
  CableDiscoveryData(const CableDiscoveryData& data);
  CableDiscoveryData& operator=(const CableDiscoveryData& other);
  bool operator==(const CableDiscoveryData& other) const;
  ~CableDiscoveryData();

  uint8_t version;
  CableEidArray client_eid;
  CableEidArray authenticator_eid;
  CableSessionPreKeyArray session_pre_key;
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_CABLE_DISCOVERY_DATA_H_
