// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud_policy_cache.h"

#include <limits>
#include <string>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
// TODO(jkummerow): remove this import when removing old DMPC test cases.
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "content/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Decodes a CloudPolicySettings object into two maps with mandatory and
// recommended settings, respectively. The implementation is generated code
// in policy/cloud_policy_generated.cc.
void DecodePolicy(const em::CloudPolicySettings& policy,
                  PolicyMap* mandatory, PolicyMap* recommended);

// The implementations of these methods are in cloud_policy_generated.cc.
Value* DecodeIntegerValue(google::protobuf::int64 value);
ListValue* DecodeStringList(const em::StringList& string_list);

class MockConfigurationPolicyProviderObserver
    : public ConfigurationPolicyProvider::Observer {
 public:
  MockConfigurationPolicyProviderObserver() {}
  virtual ~MockConfigurationPolicyProviderObserver() {}
  MOCK_METHOD0(OnUpdatePolicy, void());
  void OnProviderGoingAway() {}
};

// Tests the device management policy cache.
class CloudPolicyCacheTest : public testing::Test {
 protected:
  CloudPolicyCacheTest()
      : loop_(MessageLoop::TYPE_UI),
        ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_) {}

  void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() {
    loop_.RunAllPending();
  }

  // Creates a (signed) PolicyFetchResponse setting the given |homepage| and
  // featuring the given |timestamp| (as issued by the server).
  // Mildly hacky special feature: pass an empty string as |homepage| to get
  // a completely empty policy.
  em::PolicyFetchResponse* CreateHomepagePolicy(
      const std::string& homepage,
      const base::Time& timestamp,
      const em::PolicyOptions::PolicyMode policy_mode) {
    em::PolicyData signed_response;
    if (homepage != "") {
      em::CloudPolicySettings settings;
      em::HomepageLocationProto* homepagelocation_proto =
          settings.mutable_homepagelocation();
      homepagelocation_proto->set_homepagelocation(homepage);
      homepagelocation_proto->mutable_policy_options()->set_mode(policy_mode);
      EXPECT_TRUE(
          settings.SerializeToString(signed_response.mutable_policy_value()));
    }
    signed_response.set_timestamp(
        (timestamp - base::Time::UnixEpoch()).InMilliseconds());
    std::string serialized_signed_response;
    EXPECT_TRUE(signed_response.SerializeToString(&serialized_signed_response));

    em::PolicyFetchResponse* response = new em::PolicyFetchResponse;
    response->set_policy_data(serialized_signed_response);
    // TODO(jkummerow): Set proper certificate_chain and signature (when
    // implementing support for signature verification).
    response->set_policy_data_signature("TODO");
    response->add_certificate_chain("TODO");
    return response;
  }

  void WritePolicy(const em::PolicyFetchResponse& policy) {
    std::string data;
    em::CachedCloudPolicyResponse cached_policy;
    cached_policy.mutable_cloud_policy()->CopyFrom(policy);
    EXPECT_TRUE(cached_policy.SerializeToString(&data));
    int size = static_cast<int>(data.size());
    EXPECT_EQ(size, file_util::WriteFile(test_file(), data.c_str(), size));
  }

  // Takes ownership of |policy_response|.
  void SetPolicy(CloudPolicyCache* cache,
                 em::PolicyFetchResponse* policy_response,
                 bool expect_changed_policy) {
    scoped_ptr<em::PolicyFetchResponse> policy(policy_response);
    ConfigurationPolicyObserverRegistrar registrar;
    registrar.Init(cache->GetManagedPolicyProvider(), &observer);
    if (expect_changed_policy)
      EXPECT_CALL(observer, OnUpdatePolicy()).Times(1);
    else
      EXPECT_CALL(observer, OnUpdatePolicy()).Times(0);
    cache->SetPolicy(*policy);
    testing::Mock::VerifyAndClearExpectations(&observer);
  }

  FilePath test_file() {
    return temp_dir_.path().AppendASCII("CloudPolicyCacheTest");
  }

  const PolicyMap& mandatory_policy(const CloudPolicyCache& cache) {
    return cache.mandatory_policy_;
  }

  const PolicyMap& recommended_policy(const CloudPolicyCache& cache) {
    return cache.recommended_policy_;
  }

  MessageLoop loop_;
  MockConfigurationPolicyProviderObserver observer;

 private:
  ScopedTempDir temp_dir_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
};

TEST_F(CloudPolicyCacheTest, DecodePolicy) {
  em::CloudPolicySettings settings;
  settings.mutable_homepagelocation()->set_homepagelocation("chromium.org");
  settings.mutable_javascriptenabled()->set_javascriptenabled(true);
  settings.mutable_javascriptenabled()->mutable_policy_options()->set_mode(
      em::PolicyOptions::MANDATORY);
  settings.mutable_policyrefreshrate()->set_policyrefreshrate(5);
  settings.mutable_policyrefreshrate()->mutable_policy_options()->set_mode(
      em::PolicyOptions::RECOMMENDED);
  PolicyMap mandatory_policy;
  PolicyMap recommended_policy;
  DecodePolicy(settings, &mandatory_policy, &recommended_policy);
  PolicyMap mandatory;
  mandatory.Set(kPolicyHomepageLocation,
                Value::CreateStringValue("chromium.org"));
  mandatory.Set(kPolicyJavascriptEnabled, Value::CreateBooleanValue(true));
  PolicyMap recommended;
  recommended.Set(kPolicyPolicyRefreshRate, Value::CreateIntegerValue(5));
  EXPECT_TRUE(mandatory.Equals(mandatory_policy));
  EXPECT_TRUE(recommended.Equals(recommended_policy));
}

TEST_F(CloudPolicyCacheTest, DecodeIntegerValue) {
  const int min = std::numeric_limits<int>::min();
  const int max = std::numeric_limits<int>::max();
  scoped_ptr<Value> value(
      DecodeIntegerValue(static_cast<google::protobuf::int64>(42)));
  ASSERT_TRUE(value.get());
  FundamentalValue expected_42(42);
  EXPECT_TRUE(value->Equals(&expected_42));
  value.reset(
      DecodeIntegerValue(static_cast<google::protobuf::int64>(min - 1LL)));
  EXPECT_EQ(NULL, value.get());
  value.reset(DecodeIntegerValue(static_cast<google::protobuf::int64>(min)));
  ASSERT_TRUE(value.get());
  FundamentalValue expected_min(min);
  EXPECT_TRUE(value->Equals(&expected_min));
  value.reset(
      DecodeIntegerValue(static_cast<google::protobuf::int64>(max + 1LL)));
  EXPECT_EQ(NULL, value.get());
  value.reset(DecodeIntegerValue(static_cast<google::protobuf::int64>(max)));
  ASSERT_TRUE(value.get());
  FundamentalValue expected_max(max);
  EXPECT_TRUE(value->Equals(&expected_max));
}

TEST_F(CloudPolicyCacheTest, DecodeStringList) {
  em::StringList string_list;
  string_list.add_entries("ponies");
  string_list.add_entries("more ponies");
  scoped_ptr<ListValue> decoded(DecodeStringList(string_list));
  ListValue expected;
  expected.Append(Value::CreateStringValue("ponies"));
  expected.Append(Value::CreateStringValue("more ponies"));
  EXPECT_TRUE(decoded->Equals(&expected));
}

TEST_F(CloudPolicyCacheTest, Empty) {
  CloudPolicyCache cache(test_file());
  PolicyMap empty;
  EXPECT_TRUE(empty.Equals(mandatory_policy(cache)));
  EXPECT_TRUE(empty.Equals(recommended_policy(cache)));
  EXPECT_EQ(base::Time(), cache.last_policy_refresh_time());
}

TEST_F(CloudPolicyCacheTest, LoadNoFile) {
  CloudPolicyCache cache(test_file());
  cache.LoadFromFile();
  PolicyMap empty;
  EXPECT_TRUE(empty.Equals(mandatory_policy(cache)));
  EXPECT_EQ(base::Time(), cache.last_policy_refresh_time());
}

TEST_F(CloudPolicyCacheTest, RejectFuture) {
  scoped_ptr<em::PolicyFetchResponse> policy_response(
      CreateHomepagePolicy("", base::Time::NowFromSystemTime() +
                               base::TimeDelta::FromMinutes(5),
                           em::PolicyOptions::MANDATORY));
  WritePolicy(*policy_response);
  CloudPolicyCache cache(test_file());
  cache.LoadFromFile();
  PolicyMap empty;
  EXPECT_TRUE(empty.Equals(mandatory_policy(cache)));
  EXPECT_EQ(base::Time(), cache.last_policy_refresh_time());
}

TEST_F(CloudPolicyCacheTest, LoadWithFile) {
  scoped_ptr<em::PolicyFetchResponse> policy_response(
      CreateHomepagePolicy("", base::Time::NowFromSystemTime(),
                           em::PolicyOptions::MANDATORY));
  WritePolicy(*policy_response);
  CloudPolicyCache cache(test_file());
  cache.LoadFromFile();
  PolicyMap empty;
  EXPECT_TRUE(empty.Equals(mandatory_policy(cache)));
  EXPECT_NE(base::Time(), cache.last_policy_refresh_time());
  EXPECT_GE(base::Time::Now(), cache.last_policy_refresh_time());
}

TEST_F(CloudPolicyCacheTest, LoadWithData) {
  scoped_ptr<em::PolicyFetchResponse> policy(
      CreateHomepagePolicy("http://www.example.com",
                           base::Time::NowFromSystemTime(),
                           em::PolicyOptions::MANDATORY));
  WritePolicy(*policy);
  CloudPolicyCache cache(test_file());
  cache.LoadFromFile();
  PolicyMap expected;
  expected.Set(kPolicyHomepageLocation,
               Value::CreateStringValue("http://www.example.com"));
  EXPECT_TRUE(expected.Equals(mandatory_policy(cache)));
}

TEST_F(CloudPolicyCacheTest, SetPolicy) {
  CloudPolicyCache cache(test_file());
  em::PolicyFetchResponse* policy =
      CreateHomepagePolicy("http://www.example.com",
                           base::Time::NowFromSystemTime(),
                           em::PolicyOptions::MANDATORY);
  SetPolicy(&cache, policy, true);
  em::PolicyFetchResponse* policy2 =
      CreateHomepagePolicy("http://www.example.com",
                           base::Time::NowFromSystemTime(),
                           em::PolicyOptions::MANDATORY);
  SetPolicy(&cache, policy2, false);
  PolicyMap expected;
  expected.Set(kPolicyHomepageLocation,
               Value::CreateStringValue("http://www.example.com"));
  PolicyMap empty;
  EXPECT_TRUE(expected.Equals(mandatory_policy(cache)));
  EXPECT_TRUE(empty.Equals(recommended_policy(cache)));
  policy = CreateHomepagePolicy("http://www.example.com",
                                base::Time::NowFromSystemTime(),
                                em::PolicyOptions::RECOMMENDED);
  SetPolicy(&cache, policy, true);
  EXPECT_TRUE(expected.Equals(recommended_policy(cache)));
  EXPECT_TRUE(empty.Equals(mandatory_policy(cache)));
}

TEST_F(CloudPolicyCacheTest, ResetPolicy) {
  CloudPolicyCache cache(test_file());

  em::PolicyFetchResponse* policy =
      CreateHomepagePolicy("http://www.example.com",
                           base::Time::NowFromSystemTime(),
                           em::PolicyOptions::MANDATORY);
  SetPolicy(&cache, policy, true);
  PolicyMap expected;
  expected.Set(kPolicyHomepageLocation,
               Value::CreateStringValue("http://www.example.com"));
  EXPECT_TRUE(expected.Equals(mandatory_policy(cache)));

  em::PolicyFetchResponse* empty_policy =
      CreateHomepagePolicy("", base::Time::NowFromSystemTime(),
                           em::PolicyOptions::MANDATORY);
  SetPolicy(&cache, empty_policy, true);
  PolicyMap empty;
  EXPECT_TRUE(empty.Equals(mandatory_policy(cache)));
}

TEST_F(CloudPolicyCacheTest, PersistPolicy) {
  {
    CloudPolicyCache cache(test_file());
    scoped_ptr<em::PolicyFetchResponse> policy(
        CreateHomepagePolicy("http://www.example.com",
                             base::Time::NowFromSystemTime(),
                             em::PolicyOptions::MANDATORY));
    cache.SetPolicy(*policy);
  }

  loop_.RunAllPending();

  EXPECT_TRUE(file_util::PathExists(test_file()));
  CloudPolicyCache cache(test_file());
  cache.LoadFromFile();
  PolicyMap expected;
  expected.Set(kPolicyHomepageLocation,
               Value::CreateStringValue("http://www.example.com"));
  EXPECT_TRUE(expected.Equals(mandatory_policy(cache)));
}

TEST_F(CloudPolicyCacheTest, FreshPolicyOverride) {
  scoped_ptr<em::PolicyFetchResponse> policy(
      CreateHomepagePolicy("http://www.example.com",
                           base::Time::NowFromSystemTime(),
                           em::PolicyOptions::MANDATORY));
  WritePolicy(*policy);

  CloudPolicyCache cache(test_file());
  em::PolicyFetchResponse* updated_policy =
      CreateHomepagePolicy("http://www.chromium.org",
                           base::Time::NowFromSystemTime(),
                           em::PolicyOptions::MANDATORY);
  SetPolicy(&cache, updated_policy, true);

  cache.LoadFromFile();
  PolicyMap expected;
  expected.Set(kPolicyHomepageLocation,
               Value::CreateStringValue("http://www.chromium.org"));
  EXPECT_TRUE(expected.Equals(mandatory_policy(cache)));
}

}  // namespace policy

// ==================================================================
// Everything below this line can go when we phase out support for
// the old (trusted testing/pilot program) policy format.

// This is a (slightly updated) copy of the old
// device_management_policy_cache_unittest.cc. The new CloudPolicyCache
// supports the old DMPC's interface for now (until it is phased out), so for
// this transitional period, we keep these old test cases but apply them to the
// new implementation (CPC).

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
    em::CachedCloudPolicyResponse cached_policy;
    cached_policy.mutable_device_policy()->CopyFrom(policy);
    cached_policy.set_timestamp(timestamp.ToTimeT());
    EXPECT_TRUE(cached_policy.SerializeToString(&data));
    int size = static_cast<int>(data.size());
    EXPECT_EQ(size, file_util::WriteFile(test_file(), data.c_str(), size));
  }

  FilePath test_file() {
    return temp_dir_.path().AppendASCII("DeviceManagementPolicyCacheTest");
  }

  const DictionaryValue* device_policy(const CloudPolicyCache& cache) {
    return cache.device_policy_.get();
  }

  MessageLoop loop_;

 private:
  ScopedTempDir temp_dir_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
};

TEST_F(DeviceManagementPolicyCacheTest, Empty) {
  CloudPolicyCache cache(test_file());
  DictionaryValue empty;
  EXPECT_TRUE(empty.Equals(device_policy(cache)));
  EXPECT_EQ(base::Time(), cache.last_policy_refresh_time());
}

TEST_F(DeviceManagementPolicyCacheTest, LoadNoFile) {
  CloudPolicyCache cache(test_file());
  cache.LoadFromFile();
  DictionaryValue empty;
  EXPECT_TRUE(empty.Equals(device_policy(cache)));
  EXPECT_EQ(base::Time(), cache.last_policy_refresh_time());
}

TEST_F(DeviceManagementPolicyCacheTest, RejectFuture) {
  em::DevicePolicyResponse policy_response;
  WritePolicy(policy_response, base::Time::NowFromSystemTime() +
                               base::TimeDelta::FromMinutes(5));
  CloudPolicyCache cache(test_file());
  cache.LoadFromFile();
  DictionaryValue empty;
  EXPECT_TRUE(empty.Equals(device_policy(cache)));
  EXPECT_EQ(base::Time(), cache.last_policy_refresh_time());
}

TEST_F(DeviceManagementPolicyCacheTest, LoadWithFile) {
  em::DevicePolicyResponse policy_response;
  WritePolicy(policy_response, base::Time::NowFromSystemTime());
  CloudPolicyCache cache(test_file());
  cache.LoadFromFile();
  DictionaryValue empty;
  EXPECT_TRUE(empty.Equals(device_policy(cache)));
  EXPECT_NE(base::Time(), cache.last_policy_refresh_time());
  EXPECT_GE(base::Time::Now(), cache.last_policy_refresh_time());
}

TEST_F(DeviceManagementPolicyCacheTest, LoadWithData) {
  em::DevicePolicyResponse policy;
  AddStringPolicy(&policy, "HomepageLocation", "http://www.example.com");
  WritePolicy(policy, base::Time::NowFromSystemTime());
  CloudPolicyCache cache(test_file());
  cache.LoadFromFile();
  DictionaryValue expected;
  expected.Set("HomepageLocation",
               Value::CreateStringValue("http://www.example.com"));
  EXPECT_TRUE(expected.Equals(device_policy(cache)));
}

TEST_F(DeviceManagementPolicyCacheTest, SetDevicePolicy) {
  CloudPolicyCache cache(test_file());
  em::DevicePolicyResponse policy;
  AddStringPolicy(&policy, "HomepageLocation", "http://www.example.com");
  cache.SetDevicePolicy(policy);
  em::DevicePolicyResponse policy2;
  AddStringPolicy(&policy2, "HomepageLocation", "http://www.example.com");
  cache.SetDevicePolicy(policy2);  // Does not notify observers.
  DictionaryValue expected;
  expected.Set("HomepageLocation",
               Value::CreateStringValue("http://www.example.com"));
  EXPECT_TRUE(expected.Equals(device_policy(cache)));
}

TEST_F(DeviceManagementPolicyCacheTest, ResetPolicy) {
  CloudPolicyCache cache(test_file());

  em::DevicePolicyResponse policy;
  AddStringPolicy(&policy, "HomepageLocation", "http://www.example.com");
  cache.SetDevicePolicy(policy);
  DictionaryValue expected;
  expected.Set("HomepageLocation",
               Value::CreateStringValue("http://www.example.com"));
  EXPECT_TRUE(expected.Equals(device_policy(cache)));

  cache.SetDevicePolicy(em::DevicePolicyResponse());
  DictionaryValue empty;
  EXPECT_TRUE(empty.Equals(device_policy(cache)));
}

TEST_F(DeviceManagementPolicyCacheTest, PersistPolicy) {
  {
    CloudPolicyCache cache(test_file());
    em::DevicePolicyResponse policy;
    AddStringPolicy(&policy, "HomepageLocation", "http://www.example.com");
    cache.SetDevicePolicy(policy);
  }

  loop_.RunAllPending();

  EXPECT_TRUE(file_util::PathExists(test_file()));
  CloudPolicyCache cache(test_file());
  cache.LoadFromFile();
  DictionaryValue expected;
  expected.Set("HomepageLocation",
               Value::CreateStringValue("http://www.example.com"));
  EXPECT_TRUE(expected.Equals(device_policy(cache)));
}

TEST_F(DeviceManagementPolicyCacheTest, FreshPolicyOverride) {
  em::DevicePolicyResponse policy;
  AddStringPolicy(&policy, "HomepageLocation", "http://www.example.com");
  WritePolicy(policy, base::Time::NowFromSystemTime());

  CloudPolicyCache cache(test_file());
  em::DevicePolicyResponse updated_policy;
  AddStringPolicy(&updated_policy, "HomepageLocation",
                  "http://www.chromium.org");
  cache.SetDevicePolicy(updated_policy);

  cache.LoadFromFile();
  DictionaryValue expected;
  expected.Set("HomepageLocation",
               Value::CreateStringValue("http://www.chromium.org"));
  EXPECT_TRUE(expected.Equals(device_policy(cache)));
}

// Tests proper decoding of policy values.
class DeviceManagementPolicyCacheDecodeTest
    : public DeviceManagementPolicyCacheTestBase {
 protected:
  void DecodeAndCheck(Value* expected_value_ptr) {
    scoped_ptr<Value> expected_value(expected_value_ptr);
    scoped_ptr<Value> decoded_value(
        CloudPolicyCache::DecodeValue(value_));
    if (expected_value_ptr) {
      ASSERT_TRUE(decoded_value.get());
      EXPECT_TRUE(decoded_value->Equals(expected_value.get()));
    } else {
      ASSERT_FALSE(decoded_value.get());
    }
  }

  DictionaryValue* DecodeDevicePolicy(const em::DevicePolicyResponse policy) {
    return CloudPolicyCache::DecodeDevicePolicy(policy);
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
  scoped_ptr<Value> decoded(DecodeDevicePolicy(policy));
  DictionaryValue expected;
  expected.Set("HomepageLocation",
               Value::CreateStringValue("http://www.example.com"));
  EXPECT_TRUE(expected.Equals(decoded.get()));
}

}  // namespace policy
