// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/public/service_info.h"

#include "cast/common/public/testing/discovery_utils.h"
#include "discovery/dnssd/public/dns_sd_instance_record.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace cast {

TEST(ServiceInfoTests, ConvertValidFromDnsSd) {
  std::string instance = "InstanceId";
  discovery::DnsSdTxtRecord txt = CreateValidTxt();
  discovery::DnsSdInstanceRecord record(instance, kCastV2ServiceId,
                                        kCastV2DomainId, kEndpointV4,
                                        kEndpointV6, txt);
  ErrorOr<ServiceInfo> info = DnsSdRecordToServiceInfo(record);
  ASSERT_TRUE(info.is_value());
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_TRUE(info.value().v4_address);
  EXPECT_EQ(info.value().v4_address, kAddressV4);
  EXPECT_TRUE(info.value().v6_address);
  EXPECT_EQ(info.value().v6_address, kAddressV6);
  EXPECT_EQ(info.value().port, kPort);
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_EQ(info.value().protocol_version, kTestVersion);
  EXPECT_EQ(info.value().capabilities, kCapabilitiesParsed);
  EXPECT_EQ(info.value().status, kStatusParsed);
  EXPECT_EQ(info.value().model_name, kModelName);
  EXPECT_EQ(info.value().friendly_name, kFriendlyName);

  record = discovery::DnsSdInstanceRecord(instance, kCastV2ServiceId,
                                          kCastV2DomainId, kEndpointV4, txt);
  ASSERT_FALSE(record.address_v6());
  info = DnsSdRecordToServiceInfo(record);
  ASSERT_TRUE(info.is_value());
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_TRUE(info.value().v4_address);
  EXPECT_EQ(info.value().v4_address, kAddressV4);
  EXPECT_FALSE(info.value().v6_address);
  EXPECT_EQ(info.value().port, kPort);
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_EQ(info.value().protocol_version, kTestVersion);
  EXPECT_EQ(info.value().capabilities, kCapabilitiesParsed);
  EXPECT_EQ(info.value().status, kStatusParsed);
  EXPECT_EQ(info.value().model_name, kModelName);
  EXPECT_EQ(info.value().friendly_name, kFriendlyName);

  record = discovery::DnsSdInstanceRecord(instance, kCastV2ServiceId,
                                          kCastV2DomainId, kEndpointV6, txt);
  ASSERT_FALSE(record.address_v4());
  info = DnsSdRecordToServiceInfo(record);
  ASSERT_TRUE(info.is_value());
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_FALSE(info.value().v4_address);
  EXPECT_TRUE(info.value().v6_address);
  EXPECT_EQ(info.value().v6_address, kAddressV6);
  EXPECT_EQ(info.value().unique_id, kTestUniqueId);
  EXPECT_EQ(info.value().protocol_version, kTestVersion);
  EXPECT_EQ(info.value().capabilities, kCapabilitiesParsed);
  EXPECT_EQ(info.value().status, kStatusParsed);
  EXPECT_EQ(info.value().model_name, kModelName);
  EXPECT_EQ(info.value().friendly_name, kFriendlyName);
}

TEST(ServiceInfoTests, ConvertInvalidFromDnsSd) {
  std::string instance = "InstanceId";
  discovery::DnsSdTxtRecord txt = CreateValidTxt();
  txt.ClearValue(kUniqueIdKey);
  discovery::DnsSdInstanceRecord record(instance, kCastV2ServiceId,
                                        kCastV2DomainId, kEndpointV4,
                                        kEndpointV6, txt);
  EXPECT_TRUE(DnsSdRecordToServiceInfo(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kVersionId);
  record = discovery::DnsSdInstanceRecord(instance, kCastV2ServiceId,
                                          kCastV2DomainId, kEndpointV4,
                                          kEndpointV6, txt);
  EXPECT_TRUE(DnsSdRecordToServiceInfo(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kCapabilitiesId);
  record = discovery::DnsSdInstanceRecord(instance, kCastV2ServiceId,
                                          kCastV2DomainId, kEndpointV4,
                                          kEndpointV6, txt);
  EXPECT_TRUE(DnsSdRecordToServiceInfo(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kStatusId);
  record = discovery::DnsSdInstanceRecord(instance, kCastV2ServiceId,
                                          kCastV2DomainId, kEndpointV4,
                                          kEndpointV6, txt);
  EXPECT_TRUE(DnsSdRecordToServiceInfo(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kFriendlyNameId);
  record = discovery::DnsSdInstanceRecord(instance, kCastV2ServiceId,
                                          kCastV2DomainId, kEndpointV4,
                                          kEndpointV6, txt);
  EXPECT_TRUE(DnsSdRecordToServiceInfo(record).is_error());

  txt = CreateValidTxt();
  txt.ClearValue(kModelNameId);
  record = discovery::DnsSdInstanceRecord(instance, kCastV2ServiceId,
                                          kCastV2DomainId, kEndpointV4,
                                          kEndpointV6, txt);
  EXPECT_TRUE(DnsSdRecordToServiceInfo(record).is_error());
}

TEST(ServiceInfoTests, ConvertValidToDnsSd) {
  ServiceInfo info;
  info.v4_address = kAddressV4;
  info.v6_address = kAddressV6;
  info.port = kPort;
  info.unique_id = kTestUniqueId;
  info.protocol_version = kTestVersion;
  info.capabilities = kCapabilitiesParsed;
  info.status = kStatusParsed;
  info.model_name = kModelName;
  info.friendly_name = kFriendlyName;
  discovery::DnsSdInstanceRecord record = ServiceInfoToDnsSdRecord(info);
  EXPECT_EQ(record.instance_id(), kInstanceId);
  EXPECT_TRUE(record.address_v4());
  EXPECT_EQ(record.address_v4(), kEndpointV4);
  EXPECT_TRUE(record.address_v6());
  EXPECT_EQ(record.address_v6(), kEndpointV6);
  CompareTxtString(record.txt(), kUniqueIdKey, kTestUniqueId);
  CompareTxtString(record.txt(), kCapabilitiesId, kCapabilitiesString);
  CompareTxtString(record.txt(), kModelNameId, kModelName);
  CompareTxtString(record.txt(), kFriendlyNameId, kFriendlyName);
  CompareTxtInt(record.txt(), kVersionId, kTestVersion);
  CompareTxtInt(record.txt(), kStatusId, kStatus);

  info.v6_address = IPAddress{};
  record = ServiceInfoToDnsSdRecord(info);
  EXPECT_TRUE(record.address_v4());
  EXPECT_EQ(record.address_v4(), kEndpointV4);
  EXPECT_FALSE(record.address_v6());
  CompareTxtString(record.txt(), kUniqueIdKey, kTestUniqueId);
  CompareTxtString(record.txt(), kCapabilitiesId, kCapabilitiesString);
  CompareTxtString(record.txt(), kModelNameId, kModelName);
  CompareTxtString(record.txt(), kFriendlyNameId, kFriendlyName);
  CompareTxtInt(record.txt(), kVersionId, kTestVersion);
  CompareTxtInt(record.txt(), kStatusId, kStatus);

  info.v6_address = kAddressV6;
  info.v4_address = IPAddress{};
  record = ServiceInfoToDnsSdRecord(info);
  EXPECT_FALSE(record.address_v4());
  EXPECT_TRUE(record.address_v6());
  EXPECT_EQ(record.address_v6(), kEndpointV6);
  CompareTxtString(record.txt(), kUniqueIdKey, kTestUniqueId);
  CompareTxtString(record.txt(), kCapabilitiesId, kCapabilitiesString);
  CompareTxtString(record.txt(), kModelNameId, kModelName);
  CompareTxtString(record.txt(), kFriendlyNameId, kFriendlyName);
  CompareTxtInt(record.txt(), kVersionId, kTestVersion);
  CompareTxtInt(record.txt(), kStatusId, kStatus);
}

TEST(ServiceInfoTests, ConvertInvalidToDnsSd) {
  ServiceInfo info;
  info.unique_id = kTestUniqueId;
  info.protocol_version = kTestVersion;
  info.capabilities = kCapabilitiesParsed;
  info.status = kStatusParsed;
  info.model_name = kModelName;
  info.friendly_name = kFriendlyName;
  EXPECT_FALSE(info.IsValid());
}

TEST(ServiceInfoTests, IdentityChecks) {
  ServiceInfo info;
  info.v4_address = kAddressV4;
  info.v6_address = kAddressV6;
  info.port = kPort;
  info.unique_id = kTestUniqueId;
  info.protocol_version = kTestVersion;
  info.capabilities = kCapabilitiesParsed;
  info.status = kStatusParsed;
  info.model_name = kModelName;
  info.friendly_name = kFriendlyName;
  ASSERT_TRUE(info.IsValid());
  discovery::DnsSdInstanceRecord converted_record =
      ServiceInfoToDnsSdRecord(info);
  ErrorOr<ServiceInfo> identity_info =
      DnsSdRecordToServiceInfo(converted_record);
  ASSERT_TRUE(identity_info.is_value());
  EXPECT_EQ(identity_info.value(), info);

  discovery::DnsSdTxtRecord txt = CreateValidTxt();
  txt.SetValue(kCapabilitiesId, kCapabilitiesString);
  discovery::DnsSdInstanceRecord record(kInstanceId, kCastV2ServiceId,
                                        kCastV2DomainId, kEndpointV4,
                                        kEndpointV6, txt);
  ErrorOr<ServiceInfo> converted_info = DnsSdRecordToServiceInfo(record);
  ASSERT_TRUE(converted_info.is_value());
  ASSERT_TRUE(converted_info.value().IsValid());
  discovery::DnsSdInstanceRecord identity_record =
      ServiceInfoToDnsSdRecord(converted_info.value());
  EXPECT_EQ(identity_record, record);
}

}  // namespace cast
}  // namespace openscreen
