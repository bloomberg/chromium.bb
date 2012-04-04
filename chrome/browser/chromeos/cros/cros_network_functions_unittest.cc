// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_network_functions.h"
#include "chrome/browser/chromeos/cros/mock_chromeos_network.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;

namespace chromeos {

class CrosNetworkFunctionsTest : public testing::Test {
 public:
  CrosNetworkFunctionsTest() : argument_ghash_table_(NULL) {}

  virtual void SetUp() {
    ::g_type_init();
    MockChromeOSNetwork::Initialize();
  }

  virtual void TearDown() {
    MockChromeOSNetwork::Shutdown();
  }

  // Handles call for SetNetwork*PropertyGValue functions and copies the value.
  void OnSetNetworkPropertyGValue(const char* path,
                                  const char* property,
                                  const GValue* gvalue) {
    argument_gvalue_.reset(new GValue());
    g_value_init(argument_gvalue_.get(), G_VALUE_TYPE(gvalue));
    g_value_copy(gvalue, argument_gvalue_.get());
  }

  // Handles call for SetNetworkManagerPropertyGValue.
  void OnSetNetworkManagerPropertyGValue(const char* property,
                                         const GValue* gvalue) {
    OnSetNetworkPropertyGValue("", property, gvalue);
  }

  // Handles call for ConfigureService.
  void OnConfigureService(const char* identifier,
                          const GHashTable* properties,
                          NetworkActionCallback callback,
                          void* object) {
    // const_cast here because nothing can be done with const GHashTable*.
    argument_ghash_table_.reset(const_cast<GHashTable*>(properties));
    g_hash_table_ref(argument_ghash_table_.get());
  }

  // Does nothing.  Used as an argument.
  static void OnNetworkAction(void* object,
                              const char* path,
                              NetworkMethodErrorType error,
                              const char* error_message) {}

 protected:
  ScopedGValue argument_gvalue_;
  ScopedGHashTable argument_ghash_table_;
};

TEST_F(CrosNetworkFunctionsTest, CrosSetNetworkServiceProperty) {
  const char service_path[] = "/";
  const char property[] = "property";
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              SetNetworkServicePropertyGValue(service_path, property, _))
              .WillOnce(Invoke(
                  this, &CrosNetworkFunctionsTest::OnSetNetworkPropertyGValue));
  const char key1[] = "key1";
  const std::string string1 = "string1";
  const char key2[] = "key2";
  const std::string string2 = "string2";
  base::DictionaryValue value;
  value.SetString(key1, string1);
  value.SetString(key2, string2);
  CrosSetNetworkServiceProperty(service_path, property, value);

  // Check the argument GHashTable.
  GHashTable* ghash_table =
      static_cast<GHashTable*>(g_value_get_boxed(argument_gvalue_.get()));
  ASSERT_TRUE(ghash_table != NULL);
  EXPECT_EQ(string1, static_cast<const char*>(
      g_hash_table_lookup(ghash_table, key1)));
  EXPECT_EQ(string2, static_cast<const char*>(
      g_hash_table_lookup(ghash_table, key2)));
}

TEST_F(CrosNetworkFunctionsTest, CrosSetNetworkDeviceProperty) {
  const char device_path[] = "/";
  const char property[] = "property";
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              SetNetworkDevicePropertyGValue(device_path, property, _))
              .WillOnce(Invoke(
                  this, &CrosNetworkFunctionsTest::OnSetNetworkPropertyGValue));
  const bool kBool = true;
  const base::FundamentalValue value(kBool);
  CrosSetNetworkDeviceProperty(device_path, property, value);
  EXPECT_EQ(kBool, g_value_get_boolean(argument_gvalue_.get()));
}

TEST_F(CrosNetworkFunctionsTest, CrosSetNetworkIPConfigProperty) {
  const char ipconfig_path[] = "/";
  const char property[] = "property";
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              SetNetworkIPConfigPropertyGValue(ipconfig_path, property, _))
              .WillOnce(Invoke(
                  this, &CrosNetworkFunctionsTest::OnSetNetworkPropertyGValue));
  const int kInt = 1234;
  const base::FundamentalValue value(kInt);
  CrosSetNetworkIPConfigProperty(ipconfig_path, property, value);
  EXPECT_EQ(kInt, g_value_get_int(argument_gvalue_.get()));
}

TEST_F(CrosNetworkFunctionsTest, CrosSetNetworkManagerProperty) {
  const char property[] = "property";
  EXPECT_CALL(
      *MockChromeOSNetwork::Get(),
      SetNetworkManagerPropertyGValue(property, _))
      .WillOnce(Invoke(
          this,
          &CrosNetworkFunctionsTest::OnSetNetworkManagerPropertyGValue));
  const std::string kString = "string";
  const base::StringValue value(kString);
  CrosSetNetworkManagerProperty(property, value);
  EXPECT_EQ(kString, g_value_get_string(argument_gvalue_.get()));
}

TEST_F(CrosNetworkFunctionsTest, CrosConfigureService) {
  const char identifier[] = "identifier";
  EXPECT_CALL(*MockChromeOSNetwork::Get(),
              ConfigureService(identifier, _, &OnNetworkAction, this))
              .WillOnce(Invoke(
                  this, &CrosNetworkFunctionsTest::OnConfigureService));
  const char key1[] = "key1";
  const std::string string1 = "string1";
  const char key2[] = "key2";
  const std::string string2 = "string2";
  base::DictionaryValue value;
  value.SetString(key1, string1);
  value.SetString(key2, string2);
  CrosConfigureService(identifier, value, &OnNetworkAction, this);

  // Check the argument GHashTable.
  const GValue* string1_gvalue = static_cast<const GValue*>(
      g_hash_table_lookup(argument_ghash_table_.get(), key1));
  EXPECT_EQ(string1, g_value_get_string(string1_gvalue));
  const GValue* string2_gvalue = static_cast<const GValue*>(
      g_hash_table_lookup(argument_ghash_table_.get(), key2));
  EXPECT_EQ(string2, g_value_get_string(string2_gvalue));
}

}  // namespace chromeos
