// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state.h"

#include "base/basictypes.h"
#include "base/i18n/streaming_utf8_validator.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// StringValue that skips the DCHECK in the constructor for valid UTF8.
class TestStringValue : public base::Value {
 public:
  explicit TestStringValue(const std::string& in_value)
      : base::Value(TYPE_STRING),
        value_(in_value) {
  }

  ~TestStringValue() override {}

  // Overridden from Value:
  bool GetAsString(std::string* out_value) const override {
    if (out_value)
      *out_value = value_;
    return true;
  }

  TestStringValue* DeepCopy() const override {
    return new TestStringValue(value_);
  }

  bool Equals(const base::Value* other) const override {
    if (other->GetType() != GetType())
      return false;
    std::string lhs, rhs;
    return GetAsString(&lhs) && other->GetAsString(&rhs) && lhs == rhs;
  }

 private:
  std::string value_;
};

class NetworkStateTest : public testing::Test {
 public:
  NetworkStateTest() : network_state_("test_path") {
  }

 protected:
  bool SetStringProperty(const std::string& key, const std::string& value) {
    TestStringValue* string_value = new TestStringValue(value);
    bool res = network_state_.PropertyChanged(key, *string_value);
    properties_.SetWithoutPathExpansion(key, string_value);
    return res;
  }

  bool SignalInitialPropertiesReceived() {
    return network_state_.InitialPropertiesReceived(properties_);
  }

  NetworkState network_state_;

 private:
  base::DictionaryValue properties_;

  DISALLOW_COPY_AND_ASSIGN(NetworkStateTest);
};

}  // namespace

// Setting kNameProperty should set network name after call to
// InitialPropertiesReceived().
TEST_F(NetworkStateTest, NameAscii) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeVPN));

  std::string network_setname = "Name TEST";
  EXPECT_TRUE(SetStringProperty(shill::kNameProperty, network_setname));
  EXPECT_FALSE(SignalInitialPropertiesReceived());
  EXPECT_EQ(network_state_.name(), network_setname);
}

TEST_F(NetworkStateTest, NameAsciiWithNull) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeVPN));

  std::string network_setname = "Name TEST\x00xxx";
  std::string network_setname_result = "Name TEST";
  EXPECT_TRUE(SetStringProperty(shill::kNameProperty, network_setname));
  EXPECT_FALSE(SignalInitialPropertiesReceived());
  EXPECT_EQ(network_state_.name(), network_setname_result);
}

// Truncates invalid UTF-8
TEST_F(NetworkStateTest, NameTruncateInvalid) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeVPN));

  std::string network_setname = "SSID TEST \x01\xff!";
  std::string network_setname_result = "SSID TEST \xEF\xBF\xBD\xEF\xBF\xBD!";
  EXPECT_TRUE(SetStringProperty(shill::kNameProperty, network_setname));
  EXPECT_TRUE(SignalInitialPropertiesReceived());
  EXPECT_EQ(network_state_.name(), network_setname_result);
}

// If HexSSID doesn't exist, fallback to NameProperty.
TEST_F(NetworkStateTest, SsidFromName) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeWifi));

  std::string wifi_utf8 = "UTF-8 \u3042\u3044\u3046";
  std::string wifi_utf8_result = "UTF-8 \xE3\x81\x82\xE3\x81\x84\xE3\x81\x86";
  EXPECT_TRUE(SetStringProperty(shill::kNameProperty, wifi_utf8));
  EXPECT_FALSE(SignalInitialPropertiesReceived());
  EXPECT_EQ(network_state_.name(), wifi_utf8_result);
}

// latin1 SSID -> UTF8 SSID (Hex)
TEST_F(NetworkStateTest, SsidLatin) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeWifi));

  std::string wifi_latin1 = "latin-1 \xc0\xcb\xcc\xd6\xfb";
  std::string wifi_latin1_hex =
      base::HexEncode(wifi_latin1.c_str(), wifi_latin1.length());
  std::string wifi_latin1_result = "latin-1 \u00c0\u00cb\u00cc\u00d6\u00fb";
  EXPECT_TRUE(SetStringProperty(shill::kWifiHexSsid, wifi_latin1_hex));
  EXPECT_TRUE(SignalInitialPropertiesReceived());
  EXPECT_EQ(network_state_.name(), wifi_latin1_result);
}

// Hex SSID
TEST_F(NetworkStateTest, SsidHex) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeWifi));

  std::string wifi_hex_result = "This is HEX SSID!";
  std::string wifi_hex =
      base::HexEncode(wifi_hex_result.c_str(), wifi_hex_result.length());
  EXPECT_TRUE(SetStringProperty(shill::kWifiHexSsid, wifi_hex));
  EXPECT_TRUE(SignalInitialPropertiesReceived());
  EXPECT_EQ(network_state_.name(), wifi_hex_result);
}

// Non-UTF-8 SSID should be preserved in |raw_ssid_| field.
TEST_F(NetworkStateTest, SsidNonUtf8) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeWifi));

  std::string non_utf8_ssid = "\xc0";
  ASSERT_FALSE(base::StreamingUtf8Validator::Validate(non_utf8_ssid));

  std::vector<uint8_t> non_utf8_ssid_bytes;
  non_utf8_ssid_bytes.push_back(static_cast<uint8_t>(non_utf8_ssid.data()[0]));

  std::string wifi_hex =
      base::HexEncode(non_utf8_ssid.data(), non_utf8_ssid.size());
  EXPECT_TRUE(SetStringProperty(shill::kWifiHexSsid, wifi_hex));
  EXPECT_TRUE(SignalInitialPropertiesReceived());
  EXPECT_EQ(network_state_.raw_ssid(), non_utf8_ssid_bytes);
}

// Multiple updates for Hex SSID should work fine.
TEST_F(NetworkStateTest, SsidHexMultipleUpdates) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeWifi));

  std::string wifi_hex_result = "This is HEX SSID!";
  std::string wifi_hex =
      base::HexEncode(wifi_hex_result.c_str(), wifi_hex_result.length());
  EXPECT_TRUE(SetStringProperty(shill::kWifiHexSsid, wifi_hex));
  EXPECT_TRUE(SetStringProperty(shill::kWifiHexSsid, wifi_hex));
}

TEST_F(NetworkStateTest, CaptivePortalState) {
  std::string network_name = "test";
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeWifi));
  EXPECT_TRUE(SetStringProperty(shill::kNameProperty, network_name));
  std::string hex_ssid =
      base::HexEncode(network_name.c_str(), network_name.length());
  EXPECT_TRUE(SetStringProperty(shill::kWifiHexSsid, hex_ssid));

  // State != portal -> is_captive_portal == false
  EXPECT_TRUE(SetStringProperty(shill::kStateProperty, shill::kStateReady));
  SignalInitialPropertiesReceived();
  EXPECT_FALSE(network_state_.is_captive_portal());

  // State == portal, kPortalDetection* not set -> is_captive_portal = true
  EXPECT_TRUE(SetStringProperty(shill::kStateProperty, shill::kStatePortal));
  SignalInitialPropertiesReceived();
  EXPECT_TRUE(network_state_.is_captive_portal());

  // Set kPortalDetectionFailed* properties to states that should not trigger
  // is_captive_portal.
  SetStringProperty(shill::kPortalDetectionFailedPhaseProperty,
                    shill::kPortalDetectionPhaseUnknown);
  SetStringProperty(shill::kPortalDetectionFailedStatusProperty,
                    shill::kPortalDetectionStatusTimeout);
  SignalInitialPropertiesReceived();
  EXPECT_FALSE(network_state_.is_captive_portal());

  // Set just the phase property to the expected captive portal state.
  // is_captive_portal should still be false.
  SetStringProperty(shill::kPortalDetectionFailedPhaseProperty,
                    shill::kPortalDetectionPhaseContent);
  SignalInitialPropertiesReceived();
  EXPECT_FALSE(network_state_.is_captive_portal());

  // Set the status property to the expected captive portal state property.
  // is_captive_portal should now be true.
  SetStringProperty(shill::kPortalDetectionFailedStatusProperty,
                    shill::kPortalDetectionStatusFailure);
  SignalInitialPropertiesReceived();
  EXPECT_TRUE(network_state_.is_captive_portal());
}

}  // namespace chromeos
