// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_BLE_EID_GENERATOR_H_
#define COMPONENTS_CRYPTAUTH_BLE_EID_GENERATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/time/clock.h"

namespace cryptauth {

class BeaconSeed;
struct RemoteDevice;

// Generates ephemeral ID (EID) values for BLE communication in ProximityAuth.
//
// A peripheral-role device advertises a 4-byte advertisement with two parts: a
// 2-byte EID which is specific to the central-role device with which it intends
// to communicate, and a 2-byte EID which is specific to the peripheral-role
// device.
//
// This class uses EID seed values synced from the back-end to generate these
// EIDs.
//
// See go/proximity-auth-ble-advertising.
class EidGenerator {
 public:
  // Stores EID-related data and timestamps at which time this data becomes
  // active or inactive.
  struct DataWithTimestamp {
    DataWithTimestamp(const std::string& data,
                      const int64_t start_timestamp_ms,
                      const int64_t end_timestamp_ms);
    DataWithTimestamp(const DataWithTimestamp& other);

    bool ContainsTime(const int64_t timestamp_ms) const;
    std::string DataInHex() const;

    const std::string data;
    const int64_t start_timestamp_ms;
    const int64_t end_timestamp_ms;
  };

  // Data for both a current and adjacent EID. The current EID *must* be
  // supplied, but adjacent data may be null. Each EID consists of a 2-byte EID
  // value paired with the timestamp at which time this value becomes active or
  // inactive.
  struct EidData {
    enum AdjacentDataType { NONE, PAST, FUTURE };

    EidData(const DataWithTimestamp current_data,
            std::unique_ptr<DataWithTimestamp> adjacent_data);
    ~EidData();

    AdjacentDataType GetAdjacentDataType() const;
    std::string DataInHex() const;

    const DataWithTimestamp current_data;
    const std::unique_ptr<DataWithTimestamp> adjacent_data;
  };

  // The flag used to denote that a Bluetooth 4.0 device has sent an
  // advertisement. This flag indicates to the recipient that the sender cannot
  // act as both a central- and peripheral-role device simultaneously, so the
  // recipient should advertise back instead of initializing a connection.
  static const int8_t kBluetooth4Flag;

  static EidGenerator* GetInstance();
  virtual ~EidGenerator();

  // Generates EID data for the given EID seeds to be used as a background scan
  // filter. In the normal case, two DataWithTimestamp values are returned, one
  // for each EID seed rotation period. If data has not been synced from the
  // backend recently and EID seeds are unavailable, nullptr is returned.
  virtual std::unique_ptr<EidData> GenerateBackgroundScanFilter(
      const std::vector<BeaconSeed>& scanning_device_beacon_seeds) const;

  // Generates advertisement data for the given EID seeds. If data has not been
  // synced from the back-end recently and EID seeds are unavailable, nullptr is
  // returned.
  virtual std::unique_ptr<DataWithTimestamp> GenerateAdvertisement(
      const std::string& advertising_device_public_key,
      const std::vector<BeaconSeed>& scanning_device_beacon_seeds) const;

  // Generates all possible advertisements that could be created by a device
  // given that device's public key and the beacon seeds of the device which is
  // intended to scan for the advertisement.
  virtual std::vector<std::string> GeneratePossibleAdvertisements(
      const std::string& advertising_device_public_key,
      const std::vector<BeaconSeed>& scanning_device_beacon_seeds) const;

  // Given a list of RemoteDevices, identifies the device which could have
  // produced the supplied advertisement service data.
  virtual RemoteDevice const* IdentifyRemoteDeviceByAdvertisement(
      const std::string& advertisement_service_data,
      const std::vector<RemoteDevice>& device_list,
      const std::vector<BeaconSeed>& scanning_device_beacon_seeds) const;

 protected:
  EidGenerator();

 private:
  friend struct base::DefaultSingletonTraits<EidGenerator>;

  struct EidPeriodTimestamps {
    int64_t current_period_start_timestamp_ms;
    int64_t current_period_end_timestamp_ms;
    int64_t adjacent_period_start_timestamp_ms;
    int64_t adjacent_period_end_timestamp_ms;
  };

  class EidComputationHelper {
   public:
    virtual std::string GenerateEidDataForDevice(
        const std::string& eid_seed,
        const int64_t start_of_period_timestamp_ms,
        const std::string* extra_entropy) = 0;
  };

  class EidComputationHelperImpl : public EidComputationHelper {
   public:
    std::string GenerateEidDataForDevice(
        const std::string& eid_seed,
        const int64_t start_of_period_timestamp_ms,
        const std::string* extra_entropy) override;
  };

  static const int64_t kNumMsInEidPeriod;
  static const int64_t kNumMsInBeginningOfEidPeriod;
  static const int32_t kNumBytesInEidValue;

  EidGenerator(std::unique_ptr<EidComputationHelper> computation_helper,
               std::unique_ptr<base::Clock> clock);

  std::unique_ptr<DataWithTimestamp> GenerateAdvertisement(
      const std::string& advertising_device_public_key,
      const std::vector<BeaconSeed>& scanning_device_beacon_seeds,
      const int64_t start_of_period_timestamp_ms,
      const int64_t end_of_period_timestamp_ms) const;

  std::unique_ptr<DataWithTimestamp> GenerateEidDataWithTimestamp(
      const std::vector<BeaconSeed>& scanning_device_beacon_seeds,
      const int64_t start_of_period_timestamp_ms,
      const int64_t end_of_period_timestamp_ms) const;

  std::unique_ptr<DataWithTimestamp> GenerateEidDataWithTimestamp(
      const std::vector<BeaconSeed>& scanning_device_beacon_seeds,
      const int64_t start_of_period_timestamp_ms,
      const int64_t end_of_period_timestamp_ms,
      const std::string* extra_entropy) const;

  std::unique_ptr<std::string> GetEidSeedForPeriod(
      const std::vector<BeaconSeed>& scanning_device_beacon_seeds,
      const int64_t start_of_period_timestamp_ms) const;

  std::unique_ptr<EidPeriodTimestamps> GetEidPeriodTimestamps(
      const std::vector<BeaconSeed>& scanning_device_beacon_seeds) const;

  std::unique_ptr<EidPeriodTimestamps> GetEidPeriodTimestamps(
      const std::vector<BeaconSeed>& scanning_device_beacon_seeds,
      const bool allow_non_current_periods) const;

  std::unique_ptr<BeaconSeed> GetBeaconSeedForCurrentPeriod(
      const std::vector<BeaconSeed>& scanning_device_beacon_seeds,
      const int64_t current_time_ms) const;

  std::unique_ptr<EidPeriodTimestamps> GetClosestPeriod(
      const std::vector<BeaconSeed>& scanning_device_beacon_seeds,
      const int64_t current_time_ms) const;

  static bool IsCurrentTimeAtStartOfEidPeriod(
      const int64_t start_of_period_timestamp_ms,
      const int64_t end_of_period_timestamp_ms,
      const int64_t current_timestamp_ms);

  std::unique_ptr<base::Clock> clock_;

  std::unique_ptr<EidComputationHelper> eid_computation_helper_;

  DISALLOW_COPY_AND_ASSIGN(EidGenerator);

  friend class CryptAuthEidGeneratorTest;
  FRIEND_TEST_ALL_PREFIXES(
      CryptAuthEidGeneratorTest,
      TestGenerateEidDataForDevice_UsingRealEidComputationHelper);
  FRIEND_TEST_ALL_PREFIXES(
      CryptAuthEidGeneratorTest,
      TestGeneratePossibleAdvertisements_CurrentAndPastAdjacentPeriods);
  FRIEND_TEST_ALL_PREFIXES(
      CryptAuthEidGeneratorTest,
      testGeneratePossibleAdvertisements_CurrentAndFutureAdjacentPeriods);
  FRIEND_TEST_ALL_PREFIXES(
      CryptAuthEidGeneratorTest,
      TestGeneratePossibleAdvertisements_OnlyCurrentPeriod);
  FRIEND_TEST_ALL_PREFIXES(CryptAuthEidGeneratorTest,
                           TestGeneratePossibleAdvertisements_OnlyFuturePeriod);
  FRIEND_TEST_ALL_PREFIXES(
      CryptAuthEidGeneratorTest,
      TestGeneratePossibleAdvertisements_NoAdvertisements_SeedsTooFarInFuture);
  FRIEND_TEST_ALL_PREFIXES(CryptAuthEidGeneratorTest,
                           TestGeneratePossibleAdvertisements_OnlyPastPeriod);
  FRIEND_TEST_ALL_PREFIXES(
      CryptAuthEidGeneratorTest,
      TestGeneratePossibleAdvertisements_NoAdvertisements_SeedsTooFarInPast);
  FRIEND_TEST_ALL_PREFIXES(
      CryptAuthEidGeneratorTest,
      TestGeneratePossibleAdvertisements_NoAdvertisements_EmptySeeds);
  FRIEND_TEST_ALL_PREFIXES(CryptAuthEidGeneratorTest,
                           TestEidComputationHelperImpl_ProducesTwoByteValue);
  FRIEND_TEST_ALL_PREFIXES(CryptAuthEidGeneratorTest,
                           TestEidComputationHelperImpl_Deterministic);
  FRIEND_TEST_ALL_PREFIXES(
      CryptAuthEidGeneratorTest,
      TestEidComputationHelperImpl_ChangingSeedChangesOutput);
  FRIEND_TEST_ALL_PREFIXES(
      CryptAuthEidGeneratorTest,
      TestEidComputationHelperImpl_ChangingTimestampChangesOutput);
  FRIEND_TEST_ALL_PREFIXES(
      CryptAuthEidGeneratorTest,
      TestEidComputationHelperImpl_ChangingExtraEntropyChangesOutput);
  FRIEND_TEST_ALL_PREFIXES(
      CryptAuthEidGeneratorTest,
      TestEidComputationHelper_ChangingTimestampWithLongExtraEntropy);
  FRIEND_TEST_ALL_PREFIXES(CryptAuthEidGeneratorTest,
                           TestEidComputationHelper_EnsureTestVectorsPass);
};
}

#endif  // COMPONENTS_CRYPTAUTH_BLE_EID_GENERATOR_H_
