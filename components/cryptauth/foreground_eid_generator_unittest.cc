// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/foreground_eid_generator.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/raw_eid_generator_impl.h"
#include "components/cryptauth/remote_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
using testing::SaveArg;

namespace cryptauth {

namespace {
const int64_t kNumMsInEidPeriod =
    base::TimeDelta::FromHours(8).InMilliseconds();
const int64_t kNumMsInBeginningOfEidPeriod =
    base::TimeDelta::FromHours(2).InMilliseconds();
const int32_t kNumBytesInEidValue = 2;
const int64_t kNumMsInEidSeedPeriod =
    base::TimeDelta::FromDays(14).InMilliseconds();

// Midnight on 1/1/2020.
const int64_t kDefaultCurrentPeriodStart = 1577836800000L;
// 1:43am on 1/1/2020.
const int64_t kDefaultCurrentTime = 1577843000000L;

// The Base64 encoded values of these raw data strings are, respectively:
// "Zmlyc3RTZWVk", "c2Vjb25kU2VlZA==", "dGhpcmRTZWVk","Zm91cnRoU2VlZA==".
const std::string kFirstSeed = "firstSeed";
const std::string kSecondSeed = "secondSeed";
const std::string kThirdSeed = "thirdSeed";
const std::string kFourthSeed = "fourthSeed";

const std::string kDefaultAdvertisingDevicePublicKey = "publicKey";

static BeaconSeed CreateBeaconSeed(const std::string& data,
                                   const int64_t start_timestamp_ms,
                                   const int64_t end_timestamp_ms) {
  BeaconSeed seed;
  seed.set_data(data);
  seed.set_start_time_millis(start_timestamp_ms);
  seed.set_end_time_millis(end_timestamp_ms);
  return seed;
}

static std::string GenerateFakeEidData(
    const std::string& eid_seed,
    const int64_t start_of_period_timestamp_ms,
    const std::string* extra_entropy) {
  std::hash<std::string> string_hash;
  int64_t seed_hash = string_hash(eid_seed);
  int64_t extra_hash = extra_entropy ? string_hash(*extra_entropy) : 0;
  int64_t fake_data_xor = seed_hash ^ start_of_period_timestamp_ms ^ extra_hash;

  std::string fake_data(reinterpret_cast<const char*>(&fake_data_xor),
                        sizeof(fake_data_xor));
  fake_data.resize(kNumBytesInEidValue);

  return fake_data;
}

static std::string GenerateFakeAdvertisement(
    const std::string& scanning_device_eid_seed,
    const int64_t start_of_period_timestamp_ms,
    const std::string& advertising_device_public_key) {
  std::string fake_scanning_eid = GenerateFakeEidData(
      scanning_device_eid_seed, start_of_period_timestamp_ms, nullptr);
  std::string fake_advertising_id = GenerateFakeEidData(
      scanning_device_eid_seed, start_of_period_timestamp_ms,
      &advertising_device_public_key);
  std::string fake_advertisement;
  fake_advertisement.append(fake_scanning_eid);
  fake_advertisement.append(fake_advertising_id);
  return fake_advertisement;
}

static RemoteDevice CreateRemoteDevice(const std::string& public_key) {
  RemoteDevice remote_device;
  remote_device.public_key = public_key;
  return remote_device;
}
}  //  namespace

class CryptAuthForegroundEidGeneratorTest : public testing::Test {
 protected:
  CryptAuthForegroundEidGeneratorTest() {
    scanning_device_beacon_seeds_.push_back(CreateBeaconSeed(
        kFirstSeed, kDefaultCurrentPeriodStart - kNumMsInEidSeedPeriod,
        kDefaultCurrentPeriodStart));
    scanning_device_beacon_seeds_.push_back(
        CreateBeaconSeed(kSecondSeed, kDefaultCurrentPeriodStart,
                         kDefaultCurrentPeriodStart + kNumMsInEidSeedPeriod));
    scanning_device_beacon_seeds_.push_back(CreateBeaconSeed(
        kThirdSeed, kDefaultCurrentPeriodStart + kNumMsInEidSeedPeriod,
        kDefaultCurrentPeriodStart + 2 * kNumMsInEidSeedPeriod));
    scanning_device_beacon_seeds_.push_back(CreateBeaconSeed(
        kFourthSeed, kDefaultCurrentPeriodStart + 2 * kNumMsInEidSeedPeriod,
        kDefaultCurrentPeriodStart + 3 * kNumMsInEidSeedPeriod));
  }

  class TestRawEidGenerator : public RawEidGenerator {
   public:
    TestRawEidGenerator() {}
    ~TestRawEidGenerator() override {}

    // RawEidGenerator:
    std::string GenerateEid(const std::string& eid_seed,
                            int64_t start_of_period_timestamp_ms,
                            std::string const* extra_entropy) override {
      return GenerateFakeEidData(eid_seed, start_of_period_timestamp_ms,
                                 extra_entropy);
    }
  };

  void SetUp() override {
    test_clock_ = new base::SimpleTestClock();

    SetTestTime(kDefaultCurrentTime);

    eid_generator_.reset(
        new ForegroundEidGenerator(base::MakeUnique<TestRawEidGenerator>(),
                                   base::WrapUnique(test_clock_)));
  }

  // TODO(khorimoto): Is there an easier way to do this?
  void SetTestTime(int64_t timestamp_ms) {
    base::Time time = base::Time::UnixEpoch() +
                      base::TimeDelta::FromMilliseconds(timestamp_ms);
    test_clock_->SetNow(time);
  }

  std::unique_ptr<ForegroundEidGenerator> eid_generator_;
  base::SimpleTestClock* test_clock_;
  std::vector<BeaconSeed> scanning_device_beacon_seeds_;
};

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_StartOfPeriod_AnotherSeedInPreviousPeriod) {
  SetTestTime(kDefaultCurrentTime);

  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(
          scanning_device_beacon_seeds_);
  ASSERT_TRUE(data);
  EXPECT_EQ(data->GetAdjacentDataType(),
            ForegroundEidGenerator::EidData::AdjacentDataType::PAST);

  EXPECT_EQ(kDefaultCurrentPeriodStart, data->current_data.start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart + kNumMsInEidPeriod,
            data->current_data.end_timestamp_ms);
  EXPECT_EQ(
      GenerateFakeEidData(kSecondSeed, kDefaultCurrentPeriodStart, nullptr),
      data->current_data.data);

  ASSERT_TRUE(data->adjacent_data);
  EXPECT_EQ(kDefaultCurrentPeriodStart - kNumMsInEidPeriod,
            data->adjacent_data->start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart, data->adjacent_data->end_timestamp_ms);
  EXPECT_EQ(
      GenerateFakeEidData(
          kFirstSeed, kDefaultCurrentPeriodStart - kNumMsInEidPeriod, nullptr),
      data->adjacent_data->data);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_StartOfPeriod_NoSeedBefore) {
  SetTestTime(kDefaultCurrentTime - kNumMsInEidSeedPeriod);

  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(
          scanning_device_beacon_seeds_);
  ASSERT_TRUE(data);
  EXPECT_EQ(data->GetAdjacentDataType(),
            ForegroundEidGenerator::EidData::AdjacentDataType::NONE);

  EXPECT_EQ(kDefaultCurrentPeriodStart - kNumMsInEidSeedPeriod,
            data->current_data.start_timestamp_ms);
  EXPECT_EQ(
      kDefaultCurrentPeriodStart - kNumMsInEidSeedPeriod + kNumMsInEidPeriod,
      data->current_data.end_timestamp_ms);
  EXPECT_EQ(GenerateFakeEidData(
                kFirstSeed, kDefaultCurrentPeriodStart - kNumMsInEidSeedPeriod,
                nullptr),
            data->current_data.data);

  EXPECT_FALSE(data->adjacent_data);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_PastStartOfPeriod) {
  SetTestTime(kDefaultCurrentTime +
              base::TimeDelta::FromHours(3).InMilliseconds());

  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(
          scanning_device_beacon_seeds_);
  ASSERT_TRUE(data);
  EXPECT_EQ(data->GetAdjacentDataType(),
            ForegroundEidGenerator::EidData::AdjacentDataType::FUTURE);

  EXPECT_EQ(kDefaultCurrentPeriodStart, data->current_data.start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart + kNumMsInEidPeriod,
            data->current_data.end_timestamp_ms);
  EXPECT_EQ(
      GenerateFakeEidData(kSecondSeed, kDefaultCurrentPeriodStart, nullptr),
      data->current_data.data);

  ASSERT_TRUE(data->adjacent_data);
  EXPECT_EQ(kDefaultCurrentPeriodStart + kNumMsInEidPeriod,
            data->adjacent_data->start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart + 2 * kNumMsInEidPeriod,
            data->adjacent_data->end_timestamp_ms);
  EXPECT_EQ(
      GenerateFakeEidData(
          kSecondSeed, kDefaultCurrentPeriodStart + kNumMsInEidPeriod, nullptr),
      data->adjacent_data->data);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_EndOfPeriod) {
  SetTestTime(kDefaultCurrentPeriodStart + kNumMsInEidSeedPeriod - 1);

  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(
          scanning_device_beacon_seeds_);
  ASSERT_TRUE(data);
  EXPECT_EQ(data->GetAdjacentDataType(),
            ForegroundEidGenerator::EidData::AdjacentDataType::FUTURE);

  EXPECT_EQ(
      kDefaultCurrentPeriodStart + kNumMsInEidSeedPeriod - kNumMsInEidPeriod,
      data->current_data.start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart + kNumMsInEidSeedPeriod,
            data->current_data.end_timestamp_ms);
  EXPECT_EQ(GenerateFakeEidData(kSecondSeed,
                                kDefaultCurrentPeriodStart +
                                    kNumMsInEidSeedPeriod - kNumMsInEidPeriod,
                                nullptr),
            data->current_data.data);

  ASSERT_TRUE(data->adjacent_data);
  EXPECT_EQ(kDefaultCurrentPeriodStart + kNumMsInEidSeedPeriod,
            data->adjacent_data->start_timestamp_ms);
  EXPECT_EQ(
      kDefaultCurrentPeriodStart + kNumMsInEidSeedPeriod + kNumMsInEidPeriod,
      data->adjacent_data->end_timestamp_ms);
  EXPECT_EQ(GenerateFakeEidData(
                kThirdSeed, kDefaultCurrentPeriodStart + kNumMsInEidSeedPeriod,
                nullptr),
            data->adjacent_data->data);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_EndOfPeriod_NoSeedAfter) {
  SetTestTime(kDefaultCurrentPeriodStart + 3 * kNumMsInEidSeedPeriod - 1);

  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(
          scanning_device_beacon_seeds_);
  ASSERT_TRUE(data);
  EXPECT_EQ(data->GetAdjacentDataType(),
            ForegroundEidGenerator::EidData::AdjacentDataType::NONE);

  EXPECT_EQ(kDefaultCurrentPeriodStart + 3 * kNumMsInEidSeedPeriod -
                kNumMsInEidPeriod,
            data->current_data.start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart + 3 * kNumMsInEidSeedPeriod,
            data->current_data.end_timestamp_ms);
  EXPECT_EQ(
      GenerateFakeEidData(kFourthSeed,
                          kDefaultCurrentPeriodStart +
                              3 * kNumMsInEidSeedPeriod - kNumMsInEidPeriod,
                          nullptr),
      data->current_data.data);

  EXPECT_FALSE(data->adjacent_data);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_NoCurrentPeriodSeed) {
  SetTestTime(kDefaultCurrentPeriodStart + 4 * kNumMsInEidSeedPeriod - 1);

  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(
          scanning_device_beacon_seeds_);
  EXPECT_FALSE(data);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_EmptySeeds) {
  SetTestTime(kDefaultCurrentTime);

  std::vector<BeaconSeed> empty;
  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(empty);
  EXPECT_FALSE(data);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_InvalidSeed_PeriodNotMultipleOf8Hours) {
  SetTestTime(kDefaultCurrentTime);

  // Seed has a period of 1ms, but it should have a period of 8 hours.
  std::vector<BeaconSeed> invalid_seed_vector = {CreateBeaconSeed(
      kFirstSeed, kDefaultCurrentPeriodStart, kDefaultCurrentPeriodStart + 1)};
  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(invalid_seed_vector);
  EXPECT_FALSE(data);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_UsingRealEids) {
  test_clock_ = new base::SimpleTestClock();
  SetTestTime(kDefaultCurrentTime);

  // Use real RawEidGenerator implementation instead of test version.
  eid_generator_.reset(new ForegroundEidGenerator(
      base::MakeUnique<RawEidGeneratorImpl>(), base::WrapUnique(test_clock_)));

  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(
          scanning_device_beacon_seeds_);
  ASSERT_TRUE(data);
  EXPECT_EQ(data->GetAdjacentDataType(),
            ForegroundEidGenerator::EidData::AdjacentDataType::PAST);

  EXPECT_EQ(kDefaultCurrentPeriodStart, data->current_data.start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart + kNumMsInEidPeriod,
            data->current_data.end_timestamp_ms);
  // Since this uses the real RawEidGenerator, just make sure the data
  // exists and has the proper length.
  EXPECT_EQ((size_t)kNumBytesInEidValue, data->current_data.data.length());

  ASSERT_TRUE(data->adjacent_data);
  EXPECT_EQ(kDefaultCurrentPeriodStart - kNumMsInEidPeriod,
            data->adjacent_data->start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart, data->adjacent_data->end_timestamp_ms);
  // Since this uses the real RawEidGenerator, just make sure the data
  // exists and has the proper length.
  EXPECT_EQ((size_t)kNumBytesInEidValue, data->adjacent_data->data.length());
}

TEST_F(CryptAuthForegroundEidGeneratorTest, GenerateAdvertisementData) {
  SetTestTime(kDefaultCurrentTime);

  std::unique_ptr<DataWithTimestamp> data =
      eid_generator_->GenerateAdvertisement(kDefaultAdvertisingDevicePublicKey,
                                            scanning_device_beacon_seeds_);
  ASSERT_TRUE(data);

  EXPECT_EQ(kDefaultCurrentPeriodStart, data->start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart + kNumMsInEidPeriod,
            data->end_timestamp_ms);
  EXPECT_EQ(GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                      kDefaultAdvertisingDevicePublicKey),
            data->data);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GenerateAdvertisementData_NoSeedForPeriod) {
  SetTestTime(kDefaultCurrentTime + 4 * kNumMsInEidSeedPeriod);

  std::unique_ptr<DataWithTimestamp> data =
      eid_generator_->GenerateAdvertisement(kDefaultAdvertisingDevicePublicKey,
                                            scanning_device_beacon_seeds_);
  EXPECT_FALSE(data);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GenerateAdvertisementData_EmptySeeds) {
  SetTestTime(kDefaultCurrentTime + 4 * kNumMsInEidSeedPeriod);

  std::vector<BeaconSeed> empty;
  std::unique_ptr<DataWithTimestamp> data =
      eid_generator_->GenerateAdvertisement(kDefaultAdvertisingDevicePublicKey,
                                            empty);
  EXPECT_FALSE(data);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GeneratePossibleAdvertisements_CurrentAndPastAdjacentPeriods) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::vector<std::string> possible_advertisements =
      eid_generator_->GeneratePossibleAdvertisements(
          kDefaultAdvertisingDevicePublicKey, scanning_device_beacon_seeds_);

  EXPECT_EQ((size_t)2, possible_advertisements.size());
  EXPECT_EQ(GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                      kDefaultAdvertisingDevicePublicKey),
            possible_advertisements[0]);
  EXPECT_EQ(GenerateFakeAdvertisement(
                kFirstSeed, kDefaultCurrentPeriodStart - kNumMsInEidPeriod,
                kDefaultAdvertisingDevicePublicKey),
            possible_advertisements[1]);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       testGeneratePossibleAdvertisements_CurrentAndFutureAdjacentPeriods) {
  SetTestTime(kDefaultCurrentPeriodStart +
              base::TimeDelta::FromHours(3).InMilliseconds());

  std::vector<std::string> possible_advertisements =
      eid_generator_->GeneratePossibleAdvertisements(
          kDefaultAdvertisingDevicePublicKey, scanning_device_beacon_seeds_);

  EXPECT_EQ((size_t)2, possible_advertisements.size());
  EXPECT_EQ(GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                      kDefaultAdvertisingDevicePublicKey),
            possible_advertisements[0]);
  EXPECT_EQ(GenerateFakeAdvertisement(
                kSecondSeed, kDefaultCurrentPeriodStart + kNumMsInEidPeriod,
                kDefaultAdvertisingDevicePublicKey),
            possible_advertisements[1]);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GeneratePossibleAdvertisements_OnlyCurrentPeriod) {
  SetTestTime(kDefaultCurrentPeriodStart - kNumMsInEidSeedPeriod);

  std::vector<std::string> possible_advertisements =
      eid_generator_->GeneratePossibleAdvertisements(
          kDefaultAdvertisingDevicePublicKey, scanning_device_beacon_seeds_);

  EXPECT_EQ((size_t)1, possible_advertisements.size());
  EXPECT_EQ(GenerateFakeAdvertisement(
                kFirstSeed, kDefaultCurrentPeriodStart - kNumMsInEidSeedPeriod,
                kDefaultAdvertisingDevicePublicKey),
            possible_advertisements[0]);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GeneratePossibleAdvertisements_OnlyFuturePeriod) {
  SetTestTime(kDefaultCurrentPeriodStart - kNumMsInEidSeedPeriod -
              kNumMsInEidPeriod);

  std::vector<std::string> possible_advertisements =
      eid_generator_->GeneratePossibleAdvertisements(
          kDefaultAdvertisingDevicePublicKey, scanning_device_beacon_seeds_);

  EXPECT_EQ((size_t)1, possible_advertisements.size());
  EXPECT_EQ(GenerateFakeAdvertisement(
                kFirstSeed, kDefaultCurrentPeriodStart - kNumMsInEidSeedPeriod,
                kDefaultAdvertisingDevicePublicKey),
            possible_advertisements[0]);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GeneratePossibleAdvertisements_NoAdvertisements_SeedsTooFarInFuture) {
  SetTestTime(kDefaultCurrentPeriodStart - kNumMsInEidSeedPeriod -
              kNumMsInEidPeriod - 1);

  std::vector<std::string> possible_advertisements =
      eid_generator_->GeneratePossibleAdvertisements(
          kDefaultAdvertisingDevicePublicKey, scanning_device_beacon_seeds_);
  EXPECT_TRUE(possible_advertisements.empty());
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GeneratePossibleAdvertisements_OnlyPastPeriod) {
  SetTestTime(kDefaultCurrentPeriodStart + 3 * kNumMsInEidSeedPeriod +
              kNumMsInEidPeriod);

  std::vector<std::string> possible_advertisements =
      eid_generator_->GeneratePossibleAdvertisements(
          kDefaultAdvertisingDevicePublicKey, scanning_device_beacon_seeds_);

  EXPECT_EQ((size_t)1, possible_advertisements.size());
  EXPECT_EQ(GenerateFakeAdvertisement(kFourthSeed,
                                      kDefaultCurrentPeriodStart +
                                          3 * kNumMsInEidSeedPeriod -
                                          kNumMsInEidPeriod,
                                      kDefaultAdvertisingDevicePublicKey),
            possible_advertisements[0]);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GeneratePossibleAdvertisements_NoAdvertisements_SeedsTooFarInPast) {
  SetTestTime(kDefaultCurrentPeriodStart + 3 * kNumMsInEidSeedPeriod +
              kNumMsInEidPeriod + 1);

  std::vector<std::string> possible_advertisements =
      eid_generator_->GeneratePossibleAdvertisements(
          kDefaultAdvertisingDevicePublicKey, scanning_device_beacon_seeds_);
  EXPECT_TRUE(possible_advertisements.empty());
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       GeneratePossibleAdvertisements_NoAdvertisements_EmptySeeds) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::vector<BeaconSeed> empty;
  std::vector<std::string> possible_advertisements =
      eid_generator_->GeneratePossibleAdvertisements(
          kDefaultAdvertisingDevicePublicKey, empty);
  EXPECT_TRUE(possible_advertisements.empty());
}

TEST_F(CryptAuthForegroundEidGeneratorTest, IdentifyRemoteDevice_NoDevices) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::string service_data =
      GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                kDefaultAdvertisingDevicePublicKey);

  std::vector<RemoteDevice> device_list;
  const RemoteDevice* identified_device =
      eid_generator_->IdentifyRemoteDeviceByAdvertisement(
          service_data, device_list, scanning_device_beacon_seeds_);
  EXPECT_FALSE(identified_device);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       IdentifyRemoteDevice_OneDevice_Success) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::string service_data =
      GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                kDefaultAdvertisingDevicePublicKey);

  RemoteDevice correct_device =
      CreateRemoteDevice(kDefaultAdvertisingDevicePublicKey);
  std::vector<RemoteDevice> device_list = {correct_device};
  const RemoteDevice* identified_device =
      eid_generator_->IdentifyRemoteDeviceByAdvertisement(
          service_data, device_list, scanning_device_beacon_seeds_);
  EXPECT_EQ(correct_device.public_key, identified_device->public_key);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       IdentifyRemoteDevice_OneDevice_ServiceDataWithOneByteFlag_Success) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::string service_data =
      GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                kDefaultAdvertisingDevicePublicKey);

  // Identifying device should still succeed if there is an extra "flag" byte
  // after the first 4 bytes.
  service_data.append(
      1, static_cast<char>(ForegroundEidGenerator::kBluetooth4Flag));

  RemoteDevice correct_device =
      CreateRemoteDevice(kDefaultAdvertisingDevicePublicKey);
  std::vector<RemoteDevice> device_list = {correct_device};
  const RemoteDevice* identified_device =
      eid_generator_->IdentifyRemoteDeviceByAdvertisement(
          service_data, device_list, scanning_device_beacon_seeds_);
  EXPECT_EQ(correct_device.public_key, identified_device->public_key);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       IdentifyRemoteDevice_OneDevice_ServiceDataWithLongerFlag_Success) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::string service_data =
      GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                kDefaultAdvertisingDevicePublicKey);

  // Identifying device should still succeed if there are extra "flag" bytes
  // after the first 4 bytes.
  service_data.append("extra_flag_bytes");

  RemoteDevice correct_device =
      CreateRemoteDevice(kDefaultAdvertisingDevicePublicKey);
  std::vector<RemoteDevice> device_list = {correct_device};
  const RemoteDevice* identified_device =
      eid_generator_->IdentifyRemoteDeviceByAdvertisement(
          service_data, device_list, scanning_device_beacon_seeds_);
  EXPECT_EQ(correct_device.public_key, identified_device->public_key);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       IdentifyRemoteDevice_OneDevice_Failure) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::string service_data =
      GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                kDefaultAdvertisingDevicePublicKey);

  RemoteDevice wrong_device = CreateRemoteDevice("wrongPublicKey");
  std::vector<RemoteDevice> device_list = {wrong_device};
  const RemoteDevice* identified_device =
      eid_generator_->IdentifyRemoteDeviceByAdvertisement(
          service_data, device_list, scanning_device_beacon_seeds_);
  EXPECT_FALSE(identified_device);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       IdentifyRemoteDevice_MultipleDevices_Success) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::string service_data =
      GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                kDefaultAdvertisingDevicePublicKey);

  RemoteDevice correct_device =
      CreateRemoteDevice(kDefaultAdvertisingDevicePublicKey);
  RemoteDevice wrong_device = CreateRemoteDevice("wrongPublicKey");
  std::vector<RemoteDevice> device_list = {correct_device, wrong_device};
  const RemoteDevice* identified_device =
      eid_generator_->IdentifyRemoteDeviceByAdvertisement(
          service_data, device_list, scanning_device_beacon_seeds_);
  EXPECT_EQ(correct_device.public_key, identified_device->public_key);
}

TEST_F(CryptAuthForegroundEidGeneratorTest,
       IdentifyRemoteDevice_MultipleDevices_Failure) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::string service_data =
      GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                kDefaultAdvertisingDevicePublicKey);

  RemoteDevice wrong_device = CreateRemoteDevice("wrongPublicKey");
  std::vector<RemoteDevice> device_list = {wrong_device, wrong_device};
  const RemoteDevice* identified_device =
      eid_generator_->IdentifyRemoteDeviceByAdvertisement(
          service_data, device_list, scanning_device_beacon_seeds_);
  EXPECT_FALSE(identified_device);
}

TEST_F(CryptAuthForegroundEidGeneratorTest, DataWithTimestamp_ContainsTime) {
  DataWithTimestamp data_with_timestamp("data", /* start */ 1000L,
                                        /* end */ 2000L);
  EXPECT_FALSE(data_with_timestamp.ContainsTime(999L));
  EXPECT_TRUE(data_with_timestamp.ContainsTime(1000L));
  EXPECT_TRUE(data_with_timestamp.ContainsTime(1500L));
  EXPECT_TRUE(data_with_timestamp.ContainsTime(1999L));
  EXPECT_FALSE(data_with_timestamp.ContainsTime(2000L));
}

}  // namespace cryptauth
