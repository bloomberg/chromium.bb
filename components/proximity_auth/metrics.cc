// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/metrics.h"

#include "base/logging.h"
#include "base/md5.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/sys_byteorder.h"

namespace proximity_auth {
namespace metrics {

namespace {

// Converts the 4-byte prefix of an MD5 hash into a int32 value.
int32 DigestToInt32(const base::MD5Digest& digest) {
  // First, copy to a uint32, since byte swapping and endianness conversions
  // expect unsigned integers.
  uint32 unsigned_value;
  DCHECK_GE(arraysize(digest.a), sizeof(unsigned_value));
  memcpy(&unsigned_value, digest.a, sizeof(unsigned_value));
  unsigned_value = base::ByteSwap(base::HostToNet32(unsigned_value));

  // Then copy the resulting bit pattern to an int32 to match the datatype that
  // histograms expect.
  int32 value;
  memcpy(&value, &unsigned_value, sizeof(value));
  return value;
}

// Returns a hash of the given |name|, encoded as a 32-bit signed integer.
int32 HashDeviceModelName(const std::string& name) {
  base::MD5Digest digest;
  base::MD5Sum(name.c_str(), name.size(), &digest);
  return DigestToInt32(digest);
}

}  // namespace

const char kUnknownDeviceModel[] = "Unknown";
const int kUnknownProximityValue = 127;

void RecordAuthProximityRollingRssi(int rolling_rssi) {
  if (rolling_rssi != kUnknownProximityValue)
    rolling_rssi = std::min(50, std::max(-100, rolling_rssi));

  UMA_HISTOGRAM_SPARSE_SLOWLY("EasyUnlock.AuthProximity.RollingRssi",
                              rolling_rssi);
}

void RecordAuthProximityTransmitPowerDelta(int transmit_power_delta) {
  if (transmit_power_delta != kUnknownProximityValue)
    transmit_power_delta = std::min(50, std::max(-100, transmit_power_delta));

  UMA_HISTOGRAM_SPARSE_SLOWLY("EasyUnlock.AuthProximity.TransmitPowerDelta",
                              transmit_power_delta);
}

void RecordAuthProximityTimeSinceLastZeroRssi(
    base::TimeDelta time_since_last_zero_rssi) {
  UMA_HISTOGRAM_TIMES("EasyUnlock.AuthProximity.TimeSinceLastZeroRssi",
                      time_since_last_zero_rssi);
}

void RecordAuthProximityRemoteDeviceModelHash(const std::string& device_model) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("EasyUnlock.AuthProximity.RemoteDeviceModelHash",
                              HashDeviceModelName(device_model));
}

void RecordRemoteSecuritySettingsState(RemoteSecuritySettingsState state) {
  DCHECK(state < RemoteSecuritySettingsState::COUNT);
  UMA_HISTOGRAM_ENUMERATION(
      "EasyUnlock.RemoteLockScreenState", static_cast<int>(state),
      static_cast<int>(RemoteSecuritySettingsState::COUNT));
}

}  // namespace metrics
}  // namespace proximity_auth
