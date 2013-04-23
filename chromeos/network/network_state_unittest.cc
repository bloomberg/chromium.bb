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

class TestStringValue : public base::Value {
 public:
  TestStringValue(const std::string& in_value)
      : base::Value(TYPE_STRING),
        value_(in_value) {
  }

  ~TestStringValue() {
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

  virtual bool Equals(const Value* other) const OVERRIDE {
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
    if (!network_state_.PropertyChanged(key, TestStringValue(value)))
      return false;
    network_state_.InitialPropertiesReceived();
    return true;
  }

  NetworkState network_state_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkStateTest);
};

}  // namespace

// Seting kNameProperty should set network name after call to
// InitialPropertiesReceived() in SetStringProperty().
TEST_F(NetworkStateTest, SsidAscii) {
  std::string wifi_setname = "SSID TEST";
  std::string wifi_setname_result = "SSID TEST";
  EXPECT_TRUE(SetStringProperty(flimflam::kNameProperty, wifi_setname));
  EXPECT_EQ(network_state_.name(), wifi_setname_result);
}

TEST_F(NetworkStateTest, SsidAsciiWithNull) {
  std::string wifi_setname = "SSID TEST\x00xxx";
  std::string wifi_setname_result = "SSID TEST";
  EXPECT_TRUE(SetStringProperty(flimflam::kNameProperty, wifi_setname));
  EXPECT_EQ(network_state_.name(), wifi_setname_result);
}

// UTF8 SSID
TEST_F(NetworkStateTest, SsidUtf8) {
  std::string wifi_utf8 = "UTF-8 \u3042\u3044\u3046";
  std::string wifi_utf8_result = "UTF-8 \xE3\x81\x82\xE3\x81\x84\xE3\x81\x86";
  EXPECT_TRUE(SetStringProperty(flimflam::kNameProperty, wifi_utf8));
  EXPECT_EQ(network_state_.name(), wifi_utf8_result);
}

// Truncates invalid UTF-8
TEST_F(NetworkStateTest, SsidTruncateInvalid) {
  std::string wifi_setname2 = "SSID TEST \x01\xff!";
  std::string wifi_setname2_result = "SSID TEST \xEF\xBF\xBD\xEF\xBF\xBD!";
  EXPECT_TRUE(SetStringProperty(flimflam::kNameProperty, wifi_setname2));
  EXPECT_EQ(network_state_.name(), wifi_setname2_result);
}

// latin1 SSID -> UTF8 SSID (Hex)
TEST_F(NetworkStateTest, SsidLatin) {
  std::string wifi_latin1 = "latin-1 \xc0\xcb\xcc\xd6\xfb";
  std::string wifi_latin1_hex =
      base::HexEncode(wifi_latin1.c_str(), wifi_latin1.length());
  std::string wifi_latin1_result = "latin-1 \u00c0\u00cb\u00cc\u00d6\u00fb";
  EXPECT_TRUE(SetStringProperty(flimflam::kWifiHexSsid, wifi_latin1_hex));
  EXPECT_EQ(network_state_.name(), wifi_latin1_result);
}

// Hex SSID
TEST_F(NetworkStateTest, SsidHex) {
  std::string wifi_hex = "5468697320697320484558205353494421";
  std::string wifi_hex_result = "This is HEX SSID!";
  SetStringProperty(flimflam::kWifiHexSsid, wifi_hex);
  EXPECT_EQ(network_state_.name(), wifi_hex_result);
}

}  // namespace chromeos
