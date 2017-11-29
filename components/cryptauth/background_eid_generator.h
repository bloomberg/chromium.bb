// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_BLE_BACKGROUND_EID_GENERATOR_H_
#define COMPONENTS_CRYPTAUTH_BLE_BACKGROUND_EID_GENERATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/time/clock.h"
#include "components/cryptauth/data_with_timestamp.h"

namespace cryptauth {

class BeaconSeed;
class RawEidGenerator;

// Generates ephemeral ID (EID) values that are broadcast for background BLE
// advertisements in the ProximityAuth protocol.
//
// When advertising in background mode, we offload advertising to the hardware
// in order to conserve battery. We assume, however, that the scanning side is
// not bound by battery constraints.
//
// For the inverse of this model, in which advertising is battery-sensitive, see
// ForegroundEidGenerator.
class BackgroundEidGenerator {
 public:
  BackgroundEidGenerator();
  virtual ~BackgroundEidGenerator();

  // Returns a list of the nearest EIDs from the current time. Note that the
  // list of EIDs is sorted from earliest timestamp to latest.
  virtual std::vector<DataWithTimestamp> GenerateNearestEids(
      const std::vector<BeaconSeed>& beacon_seed) const;

 private:
  friend class CryptAuthBackgroundEidGeneratorTest;
  BackgroundEidGenerator(std::unique_ptr<RawEidGenerator> raw_eid_generator,
                         base::Clock* clock);

  // Helper function to generate the EID for any |timestamp_ms|, properly
  // calculating the start of the period. Returns nullptr if |timestamp_ms| is
  // outside of range of |beacon_seeds|.
  std::unique_ptr<DataWithTimestamp> GenerateEid(
      int64_t timestamp_ms,
      const std::vector<BeaconSeed>& beacon_seeds) const;

  std::unique_ptr<RawEidGenerator> raw_eid_generator_;
  base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundEidGenerator);
};

}  // cryptauth

#endif  // COMPONENTS_CRYPTAUTH_BLE_BACKGROUND_EID_GENERATOR_H_
