// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_management_policy_cache.h"

#include <limits>
#include <string>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Wraps base functionaly for the test cases.
class DeviceManagementPolicyCacheTestBase : public testing::Test {
 protected:
  // Add a string policy setting to a policy response message.
  void AddStringPolicy(em::DevicePolicyResponse* policy,
                       const std::string& name,
                       const std::string& value) {
    em::DevicePolicySetting* setting = policy->add_setting();
    setting->set_policy_key(kChromeDevicePolicySettingKey);
    em::GenericSetting* policy_value = setting->mutable_policy_value();
    em::GenericNamedValue* named_value = policy_value->add_named_value();
    named_value->set_name(name);
    em::GenericValue* value_container = named_value->mutable_value();
    value_container->set_value_type(em::GenericValue::VALUE_TYPE_STRING);
    value_container->set_string_value(value);
  }
};

// Tests the device management policy cache.
class DeviceManagementPolicyCacheTest
    : public DeviceManagementPolicyCacheTestBase {
 protected:
  DeviceManagementPolicyCacheTest()
      : loop_(MessageLoop::TYPE_UI),
        ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_) {}

  void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() {
    loop_.RunAllPending();
  }

  void WritePolicy(const em::DevicePolicyResponse& policy,
                   const base::Time& timestamp) {
    std::string data;
    em::CachedDevicePolicyResponse cached_policy;
    cached_policy.mutable_policy()->CopyFrom(policy);
    cached_policy.set_timestamp(timestamp.ToInternalValue());
    EXPECT_TRUE(cached_policy.SerializeToString(&data));
    int size = static_cast<int>(data.size());
    EXPECT_EQ(size, file_util::WriteFile(test_file(), data.c_str(), size));
  }

  FilePath test_file() {
    return temp_dir_.path().AppendASCII("DeviceManagementPolicyCacheTest");
  }

 protected:
  MessageLoop loop_;

 private:
  ScopedTempDir temp_dir_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
};

TEST_F(DeviceManagementPolicyCacheTest, Empty) {
  DeviceManagementPolicyCache cache(test_file());
  DictionaryValue empty;
  scoped_ptr<Value> policy(cache.GetPolicy());
  EXPECT_TRUE(empty.Equals(policy.get()));
  EXPECT_EQ(base::Time(), cache.last_policy_refresh_time());
}

TEST_F(DeviceManagementPolicyCacheTest, LoadNoFile) {
  DeviceManagementPolicyCache cache(test_file());
  cache.LoadPolicyFromFile();
  DictionaryValue empty;
  scoped_ptr<Value> policy(cache.GetPolicy());
  EXPECT_TRUE(empty.Equals(policy.get()));
  EXPECT_EQ(base::Time(), cache.last_policy_refresh_time());
}

TEST_F(DeviceManagementPolicyCacheTest, RejectFuture) {
  em::DevicePolicyResponse policy_response;
  WritePolicy(policy_response, base::Time::NowFromSystemTime() +
                               base::TimeDelta::FromMinutes(5));
  DeviceManagementPolicyCache cache(test_file());
  cache.LoadPolicyFromFile();
  DictionaryValue empty;
  scoped_ptr<Value> policy(cache.GetPolicy());
  EXPECT_TRUE(empty.Equals(policy.get()));
  EXPECT_EQ(base::Time(), cache.last_policy_refresh_time());
}

TEST_F(DeviceManagementPolicyCacheTest, LoadWithFile) {
  em::DevicePolicyResponse policy_response;
  WritePolicy(policy_response, base::Time::NowFromSystemTime());
  DeviceManagementPolicyCache cache(test_file());
  cache.LoadPolicyFromFile();
  DictionaryValue empty;
  scoped_ptr<Value> policy(cache.GetPolicy());
  EXPECT_TRUE(empty.Equals(policy.get()));
  EXPECT_NE(base::Time(), cache.last_policy_refresh_time());
  EXPECT_GE(base::Time::Now(), cache.last_policy_refresh_time());
}

TEST_F(DeviceManagementPolicyCacheTest, LoadWithData) {
  em::DevicePolicyResponse policy;
  AddStringPolicy(&policy, "HomepageLocation", "http://www.example.com");
  WritePolicy(policy, base::Time::NowFromSystemTime());
  DeviceManagementPolicyCache cache(test_file());
  cache.LoadPolicyFromFile();
  DictionaryValue expected;
  expected.Set("HomepageLocation",
               Value::CreateStringValue("http://www.example.com"));
  scoped_ptr<Value> policy_value(cache.GetPolicy());
  EXPECT_TRUE(expected.Equals(policy_value.get()));
}

TEST_F(DeviceManagementPolicyCacheTest, SetPolicy) {
  DeviceManagementPolicyCache cache(test_file());
  em::DevicePolicyResponse policy;
  AddStringPolicy(&policy, "HomepageLocation", "http://www.example.com");
  EXPECT_TRUE(cache.SetPolicy(policy));
  em::DevicePolicyResponse policy2;
  AddStringPolicy(&policy2, "HomepageLocation", "http://www.example.com");
  EXPECT_FALSE(cache.SetPolicy(policy2));
  DictionaryValue expected;
  expected.Set("HomepageLocation",
               Value::CreateStringValue("http://www.example.com"));
  scoped_ptr<Value> policy_value(cache.GetPolicy());
  EXPECT_TRUE(expected.Equals(policy_value.get()));
}

TEST_F(DeviceManagementPolicyCacheTest, ResetPolicy) {
  DeviceManagementPolicyCache cache(test_file());

  em::DevicePolicyResponse policy;
  AddStringPolicy(&policy, "HomepageLocation", "http://www.example.com");
  EXPECT_TRUE(cache.SetPolicy(policy));
  DictionaryValue expected;
  expected.Set("HomepageLocation",
               Value::CreateStringValue("http://www.example.com"));
  scoped_ptr<Value> policy_value(cache.GetPolicy());
  EXPECT_TRUE(expected.Equals(policy_value.get()));

  EXPECT_TRUE(cache.SetPolicy(em::DevicePolicyResponse()));
  policy_value.reset(cache.GetPolicy());
  DictionaryValue empty;
  EXPECT_TRUE(empty.Equals(policy_value.get()));
}

TEST_F(DeviceManagementPolicyCacheTest, PersistPolicy) {
  {
    DeviceManagementPolicyCache cache(test_file());
    em::DevicePolicyResponse policy;
    AddStringPolicy(&policy, "HomepageLocation", "http://www.example.com");
    cache.SetPolicy(policy);
  }

  loop_.RunAllPending();

  EXPECT_TRUE(file_util::PathExists(test_file()));
  DeviceManagementPolicyCache cache(test_file());
  cache.LoadPolicyFromFile();
  DictionaryValue expected;
  expected.Set("HomepageLocation",
               Value::CreateStringValue("http://www.example.com"));
  scoped_ptr<Value> policy_value(cache.GetPolicy());
  EXPECT_TRUE(expected.Equals(policy_value.get()));
}

TEST_F(DeviceManagementPolicyCacheTest, FreshPolicyOverride) {
  em::DevicePolicyResponse policy;
  AddStringPolicy(&policy, "HomepageLocation", "http://www.example.com");
  WritePolicy(policy, base::Time::NowFromSystemTime());

  DeviceManagementPolicyCache cache(test_file());
  em::DevicePolicyResponse updated_policy;
  AddStringPolicy(&updated_policy, "HomepageLocation",
                  "http://www.chromium.org");
  EXPECT_TRUE(cache.SetPolicy(updated_policy));

  cache.LoadPolicyFromFile();
  DictionaryValue expected;
  expected.Set("HomepageLocation",
               Value::CreateStringValue("http://www.chromium.org"));
  scoped_ptr<Value> policy_value(cache.GetPolicy());
  EXPECT_TRUE(expected.Equals(policy_value.get()));
}

// Tests proper decoding of policy values.
class DeviceManagementPolicyCacheDecodeTest
    : public DeviceManagementPolicyCacheTestBase {
 protected:
  void DecodeAndCheck(Value* expected_value_ptr) {
    scoped_ptr<Value> expected_value(expected_value_ptr);
    scoped_ptr<Value> decoded_value(
        DeviceManagementPolicyCache::DecodeValue(value_));
    if (expected_value_ptr) {
      ASSERT_TRUE(decoded_value.get());
      EXPECT_TRUE(decoded_value->Equals(expected_value.get()));
    } else {
      ASSERT_FALSE(decoded_value.get());
    }
  }

  em::GenericValue value_;
};

TEST_F(DeviceManagementPolicyCacheDecodeTest, Bool) {
  value_.set_value_type(em::GenericValue::VALUE_TYPE_BOOL);
  value_.set_bool_value(true);
  DecodeAndCheck(Value::CreateBooleanValue(true));
}

TEST_F(DeviceManagementPolicyCacheDecodeTest, Int64) {
  value_.set_value_type(em::GenericValue::VALUE_TYPE_INT64);
  value_.set_int64_value(42);
  DecodeAndCheck(Value::CreateIntegerValue(42));
}

TEST_F(DeviceManagementPolicyCacheDecodeTest, Int64Overflow) {
  const int min = std::numeric_limits<int>::min();
  const int max = std::numeric_limits<int>::max();
  value_.set_value_type(em::GenericValue::VALUE_TYPE_INT64);
  value_.set_int64_value(min - 1LL);
  DecodeAndCheck(NULL);
  value_.set_int64_value(max + 1LL);
  DecodeAndCheck(NULL);
  value_.set_int64_value(min);
  DecodeAndCheck(Value::CreateIntegerValue(min));
  value_.set_int64_value(max);
  DecodeAndCheck(Value::CreateIntegerValue(max));
}

TEST_F(DeviceManagementPolicyCacheDecodeTest, String) {
  value_.set_value_type(em::GenericValue::VALUE_TYPE_STRING);
  value_.set_string_value("ponies!");
  DecodeAndCheck(Value::CreateStringValue("ponies!"));
}

TEST_F(DeviceManagementPolicyCacheDecodeTest, Double) {
  value_.set_value_type(em::GenericValue::VALUE_TYPE_DOUBLE);
  value_.set_double_value(0.42L);
  DecodeAndCheck(Value::CreateDoubleValue(0.42L));
}

TEST_F(DeviceManagementPolicyCacheDecodeTest, Bytes) {
  std::string data("binary ponies.");
  value_.set_value_type(em::GenericValue::VALUE_TYPE_BYTES);
  value_.set_bytes_value(data);
  DecodeAndCheck(
      BinaryValue::CreateWithCopiedBuffer(data.c_str(), data.size()));
}

TEST_F(DeviceManagementPolicyCacheDecodeTest, BoolArray) {
  value_.set_value_type(em::GenericValue::VALUE_TYPE_BOOL_ARRAY);
  value_.add_bool_array(false);
  value_.add_bool_array(true);
  ListValue* list = new ListValue;
  list->Append(Value::CreateBooleanValue(false));
  list->Append(Value::CreateBooleanValue(true));
  DecodeAndCheck(list);
}

TEST_F(DeviceManagementPolicyCacheDecodeTest, Int64Array) {
  value_.set_value_type(em::GenericValue::VALUE_TYPE_INT64_ARRAY);
  value_.add_int64_array(42);
  value_.add_int64_array(17);
  ListValue* list = new ListValue;
  list->Append(Value::CreateIntegerValue(42));
  list->Append(Value::CreateIntegerValue(17));
  DecodeAndCheck(list);
}

TEST_F(DeviceManagementPolicyCacheDecodeTest, StringArray) {
  value_.set_value_type(em::GenericValue::VALUE_TYPE_STRING_ARRAY);
  value_.add_string_array("ponies");
  value_.add_string_array("more ponies");
  ListValue* list = new ListValue;
  list->Append(Value::CreateStringValue("ponies"));
  list->Append(Value::CreateStringValue("more ponies"));
  DecodeAndCheck(list);
}

TEST_F(DeviceManagementPolicyCacheDecodeTest, DoubleArray) {
  value_.set_value_type(em::GenericValue::VALUE_TYPE_DOUBLE_ARRAY);
  value_.add_double_array(0.42L);
  value_.add_double_array(0.17L);
  ListValue* list = new ListValue;
  list->Append(Value::CreateDoubleValue(0.42L));
  list->Append(Value::CreateDoubleValue(0.17L));
  DecodeAndCheck(list);
}

TEST_F(DeviceManagementPolicyCacheDecodeTest, DecodePolicy) {
  em::DevicePolicyResponse policy;
  AddStringPolicy(&policy, "HomepageLocation", "http://www.example.com");
  scoped_ptr<Value> decoded(DeviceManagementPolicyCache::DecodePolicy(policy));
  DictionaryValue expected;
  expected.Set("HomepageLocation",
               Value::CreateStringValue("http://www.example.com"));
  EXPECT_TRUE(expected.Equals(decoded.get()));
}

}  // namespace policy
