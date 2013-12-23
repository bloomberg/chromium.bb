// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state.h"

#include "base/basictypes.h"
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

  virtual ~TestStringValue() {
  }

  // Overridden from Value:
  virtual bool GetAsString(std::string* out_value) const OVERRIDE {
    if (out_value)
      *out_value = value_;
    return true;
  }

  virtual TestStringValue* DeepCopy() const OVERRIDE {
    return new TestStringValue(value_);
  }

  virtual bool Equals(const base::Value* other) const OVERRIDE {
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
  EXPECT_FALSE(SetStringProperty(shill::kWifiHexSsid, wifi_latin1_hex));
  EXPECT_TRUE(SignalInitialPropertiesReceived());
  EXPECT_EQ(network_state_.name(), wifi_latin1_result);
}

// Hex SSID
TEST_F(NetworkStateTest, SsidHex) {
  EXPECT_TRUE(SetStringProperty(shill::kTypeProperty, shill::kTypeWifi));

  std::string wifi_hex_result = "This is HEX SSID!";
  std::string wifi_hex =
      base::HexEncode(wifi_hex_result.c_str(), wifi_hex_result.length());
  EXPECT_FALSE(SetStringProperty(shill::kWifiHexSsid, wifi_hex));
  EXPECT_TRUE(SignalInitialPropertiesReceived());
  EXPECT_EQ(network_state_.name(), wifi_hex_result);
}

}  // namespace chromeos
