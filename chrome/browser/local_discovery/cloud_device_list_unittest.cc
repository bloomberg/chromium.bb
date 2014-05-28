// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/cloud_device_list.h"

#include <set>

#include "base/json/json_reader.h"
#include "chrome/browser/local_discovery/cloud_device_list_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::SaveArg;
using testing::StrictMock;
using testing::_;

namespace local_discovery {

namespace {

const char kSampleSuccessResponseOAuth[] =
    "{"
    "\"kind\": \"clouddevices#devicesListResponse\","
    "\"devices\": [{"
    "  \"kind\": \"clouddevices#device\","
    "  \"id\": \"someID\","
    "  \"deviceKind\": \"someType\","
    "  \"creationTimeMs\": \"123\","
    "  \"systemName\": \"someDisplayName\","
    "  \"owner\": \"user@domain.com\","
    "  \"description\": \"someDescription\","
    "  \"state\": {"
    "  \"base\": {"
    "    \"connectionStatus\": \"offline\""
    "   }"
    "  },"
    "  \"channel\": {"
    "  \"supportedType\": \"xmpp\""
    "  },"
    "  \"personalizedInfo\": {"
    "   \"maxRole\": \"owner\""
    "  }}]}";

class MockDelegate : public CloudDeviceListDelegate {
 public:
  MOCK_METHOD1(OnDeviceListReady, void(const DeviceList&));
  MOCK_METHOD0(OnDeviceListUnavailable, void());
};

TEST(CloudDeviceListTest, Params) {
  CloudDeviceList device_list(NULL);
  EXPECT_EQ(GURL("https://www.googleapis.com/clouddevices/v1/devices"),
            device_list.GetURL());
  EXPECT_EQ("https://www.googleapis.com/auth/clouddevices",
            device_list.GetOAuthScope());
  EXPECT_EQ(net::URLFetcher::GET, device_list.GetRequestType());
  EXPECT_TRUE(device_list.GetExtraRequestHeaders().empty());
}

TEST(CloudDeviceListTest, Parsing) {
  StrictMock<MockDelegate> delegate;
  CloudDeviceList device_list(&delegate);
  CloudDeviceListDelegate::DeviceList devices;
  EXPECT_CALL(delegate, OnDeviceListReady(_)).WillOnce(SaveArg<0>(&devices));

  scoped_ptr<base::Value> value(
      base::JSONReader::Read(kSampleSuccessResponseOAuth));
  const base::DictionaryValue* dictionary = NULL;
  ASSERT_TRUE(value->GetAsDictionary(&dictionary));
  device_list.OnGCDAPIFlowComplete(*dictionary);

  Mock::VerifyAndClear(&delegate);

  std::set<std::string> ids_found;
  std::set<std::string> ids_expected;
  ids_expected.insert("someID");

  for (size_t i = 0; i != devices.size(); ++i) {
    ids_found.insert(devices[i].id);
  }

  ASSERT_EQ(ids_expected, ids_found);

  EXPECT_EQ("someID", devices[0].id);
  EXPECT_EQ("someDisplayName", devices[0].display_name);
  EXPECT_EQ("someDescription", devices[0].description);
  EXPECT_EQ("someType", devices[0].type);
}

}  // namespace

}  // namespace local_discovery
