// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_policy_cache.h"

#include <limits>
#include <string>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/values.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "chrome/browser/policy/proto/old_generic_format.pb.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using testing::_;

namespace policy {

// Decodes a CloudPolicySettings object into two maps with mandatory and
// recommended settings, respectively. The implementation is generated code
// in policy/cloud_policy_generated.cc.
void DecodePolicy(const em::CloudPolicySettings& policy,
                  PolicyMap* mandatory, PolicyMap* recommended);

// The implementations of these methods are in cloud_policy_generated.cc.
Value* DecodeIntegerValue(google::protobuf::int64 value);
ListValue* DecodeStringList(const em::StringList& string_list);

class MockCloudPolicyCacheBaseObserver
    : public CloudPolicyCacheBase::Observer {
 public:
  MockCloudPolicyCacheBaseObserver() {}
  virtual ~MockCloudPolicyCacheBaseObserver() {}
  MOCK_METHOD1(OnCacheUpdate, void(CloudPolicyCacheBase*));
  void OnCacheGoingAway(CloudPolicyCacheBase*) {}
};

// Tests the device management policy cache.
class UserPolicyCacheTest : public testing::Test {
 protected:
  UserPolicyCacheTest()
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
    // TODO(jkummerow): Set proper new_public_key and signature (when
    // implementing support for signature verification).
    response->set_policy_data_signature("TODO");
    response->set_new_public_key("TODO");
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
  void SetPolicy(UserPolicyCache* cache,
                 em::PolicyFetchResponse* policy_response) {
    EXPECT_CALL(observer_, OnCacheUpdate(_)).Times(1);
    cache->AddObserver(&observer_);

    scoped_ptr<em::PolicyFetchResponse> policy(policy_response);
    cache->SetPolicy(*policy);
    cache->SetReady();
    testing::Mock::VerifyAndClearExpectations(&observer_);

    cache->RemoveObserver(&observer_);
  }

  void SetReady(UserPolicyCache* cache) {
    cache->SetReady();
  }

  FilePath test_file() {
    return temp_dir_.path().AppendASCII("UserPolicyCacheTest");
  }

  const PolicyMap& mandatory_policy(const UserPolicyCache& cache) {
    return cache.mandatory_policy_;
  }

  const PolicyMap& recommended_policy(const UserPolicyCache& cache) {
    return cache.recommended_policy_;
  }

  MessageLoop loop_;
  MockCloudPolicyCacheBaseObserver observer_;

 private:
  ScopedTempDir temp_dir_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
};

TEST_F(UserPolicyCacheTest, DecodePolicy) {
  em::CloudPolicySettings settings;
  settings.mutable_homepagelocation()->set_homepagelocation("chromium.org");
  settings.mutable_javascriptenabled()->set_javascriptenabled(true);
  settings.mutable_javascriptenabled()->mutable_policy_options()->set_mode(
      em::PolicyOptions::MANDATORY);
  settings.mutable_policyrefreshrate()->set_policyrefreshrate(5);
  settings.mutable_policyrefreshrate()->mutable_policy_options()->set_mode(
      em::PolicyOptions::RECOMMENDED);
  settings.mutable_showhomebutton()->set_showhomebutton(true);
  settings.mutable_showhomebutton()->mutable_policy_options()->set_mode(
      em::PolicyOptions::UNSET);
#ifdef NDEBUG
  // Setting an invalid PolicyMode enum value triggers a DCHECK in protobuf.
  settings.mutable_homepageisnewtabpage()->set_homepageisnewtabpage(true);
  settings.mutable_homepageisnewtabpage()->mutable_policy_options()->set_mode(
      static_cast<em::PolicyOptions::PolicyMode>(-1));
#endif
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

TEST_F(UserPolicyCacheTest, DecodeIntegerValue) {
  const int min = std::numeric_limits<int>::min();
  const int max = std::numeric_limits<int>::max();
  scoped_ptr<Value> value(
      DecodeIntegerValue(static_cast<google::protobuf::int64>(42)));
  ASSERT_TRUE(value.get());
  base::FundamentalValue expected_42(42);
  EXPECT_TRUE(value->Equals(&expected_42));
  value.reset(
      DecodeIntegerValue(static_cast<google::protobuf::int64>(min - 1LL)));
  EXPECT_EQ(NULL, value.get());
  value.reset(DecodeIntegerValue(static_cast<google::protobuf::int64>(min)));
  ASSERT_TRUE(value.get());
  base::FundamentalValue expected_min(min);
  EXPECT_TRUE(value->Equals(&expected_min));
  value.reset(
      DecodeIntegerValue(static_cast<google::protobuf::int64>(max + 1LL)));
  EXPECT_EQ(NULL, value.get());
  value.reset(DecodeIntegerValue(static_cast<google::protobuf::int64>(max)));
  ASSERT_TRUE(value.get());
  base::FundamentalValue expected_max(max);
  EXPECT_TRUE(value->Equals(&expected_max));
}

TEST_F(UserPolicyCacheTest, DecodeStringList) {
  em::StringList string_list;
  string_list.add_entries("ponies");
  string_list.add_entries("more ponies");
  scoped_ptr<ListValue> decoded(DecodeStringList(string_list));
  ListValue expected;
  expected.Append(Value::CreateStringValue("ponies"));
  expected.Append(Value::CreateStringValue("more ponies"));
  EXPECT_TRUE(decoded->Equals(&expected));
}

TEST_F(UserPolicyCacheTest, Empty) {
  UserPolicyCache cache(test_file(), false  /* wait_for_policy_fetch */);
  PolicyMap empty;
  EXPECT_TRUE(empty.Equals(mandatory_policy(cache)));
  EXPECT_TRUE(empty.Equals(recommended_policy(cache)));
  EXPECT_EQ(base::Time(), cache.last_policy_refresh_time());
}

TEST_F(UserPolicyCacheTest, LoadNoFile) {
  UserPolicyCache cache(test_file(), false  /* wait_for_policy_fetch */);
  cache.Load();
  loop_.RunAllPending();
  PolicyMap empty;
  EXPECT_TRUE(empty.Equals(mandatory_policy(cache)));
  EXPECT_EQ(base::Time(), cache.last_policy_refresh_time());
}

TEST_F(UserPolicyCacheTest, RejectFuture) {
  scoped_ptr<em::PolicyFetchResponse> policy_response(
      CreateHomepagePolicy("", base::Time::NowFromSystemTime() +
                               base::TimeDelta::FromMinutes(5),
                           em::PolicyOptions::MANDATORY));
  WritePolicy(*policy_response);
  UserPolicyCache cache(test_file(), false  /* wait_for_policy_fetch */);
  cache.Load();
  loop_.RunAllPending();
  PolicyMap empty;
  EXPECT_TRUE(empty.Equals(mandatory_policy(cache)));
  EXPECT_EQ(base::Time(), cache.last_policy_refresh_time());
}

TEST_F(UserPolicyCacheTest, LoadWithFile) {
  scoped_ptr<em::PolicyFetchResponse> policy_response(
      CreateHomepagePolicy("", base::Time::NowFromSystemTime(),
                           em::PolicyOptions::MANDATORY));
  WritePolicy(*policy_response);
  UserPolicyCache cache(test_file(), false  /* wait_for_policy_fetch */);
  cache.Load();
  loop_.RunAllPending();
  PolicyMap empty;
  EXPECT_TRUE(empty.Equals(mandatory_policy(cache)));
  EXPECT_NE(base::Time(), cache.last_policy_refresh_time());
  EXPECT_GE(base::Time::Now(), cache.last_policy_refresh_time());
}

TEST_F(UserPolicyCacheTest, LoadWithData) {
  scoped_ptr<em::PolicyFetchResponse> policy(
      CreateHomepagePolicy("http://www.example.com",
                           base::Time::NowFromSystemTime(),
                           em::PolicyOptions::MANDATORY));
  WritePolicy(*policy);
  UserPolicyCache cache(test_file(), false  /* wait_for_policy_fetch */);
  cache.Load();
  loop_.RunAllPending();
  PolicyMap expected;
  expected.Set(kPolicyHomepageLocation,
               Value::CreateStringValue("http://www.example.com"));
  EXPECT_TRUE(expected.Equals(mandatory_policy(cache)));
}

TEST_F(UserPolicyCacheTest, SetPolicy) {
  UserPolicyCache cache(test_file(), false  /* wait_for_policy_fetch */);
  em::PolicyFetchResponse* policy =
      CreateHomepagePolicy("http://www.example.com",
                           base::Time::NowFromSystemTime(),
                           em::PolicyOptions::MANDATORY);
  SetPolicy(&cache, policy);
  em::PolicyFetchResponse* policy2 =
      CreateHomepagePolicy("http://www.example.com",
                           base::Time::NowFromSystemTime(),
                           em::PolicyOptions::MANDATORY);
  SetPolicy(&cache, policy2);
  PolicyMap expected;
  expected.Set(kPolicyHomepageLocation,
               Value::CreateStringValue("http://www.example.com"));
  PolicyMap empty;
  EXPECT_TRUE(expected.Equals(mandatory_policy(cache)));
  EXPECT_TRUE(empty.Equals(recommended_policy(cache)));
  policy = CreateHomepagePolicy("http://www.example.com",
                                base::Time::NowFromSystemTime(),
                                em::PolicyOptions::RECOMMENDED);
  SetPolicy(&cache, policy);
  EXPECT_TRUE(expected.Equals(recommended_policy(cache)));
  EXPECT_TRUE(empty.Equals(mandatory_policy(cache)));
}

TEST_F(UserPolicyCacheTest, ResetPolicy) {
  UserPolicyCache cache(test_file(), false  /* wait_for_policy_fetch */);

  em::PolicyFetchResponse* policy =
      CreateHomepagePolicy("http://www.example.com",
                           base::Time::NowFromSystemTime(),
                           em::PolicyOptions::MANDATORY);
  SetPolicy(&cache, policy);
  PolicyMap expected;
  expected.Set(kPolicyHomepageLocation,
               Value::CreateStringValue("http://www.example.com"));
  EXPECT_TRUE(expected.Equals(mandatory_policy(cache)));

  em::PolicyFetchResponse* empty_policy =
      CreateHomepagePolicy("", base::Time::NowFromSystemTime(),
                           em::PolicyOptions::MANDATORY);
  SetPolicy(&cache, empty_policy);
  PolicyMap empty;
  EXPECT_TRUE(empty.Equals(mandatory_policy(cache)));
}

TEST_F(UserPolicyCacheTest, PersistPolicy) {
  {
    UserPolicyCache cache(test_file(), false  /* wait_for_policy_fetch */);
    scoped_ptr<em::PolicyFetchResponse> policy(
        CreateHomepagePolicy("http://www.example.com",
                             base::Time::NowFromSystemTime(),
                             em::PolicyOptions::MANDATORY));
    cache.SetPolicy(*policy);
  }

  loop_.RunAllPending();

  EXPECT_TRUE(file_util::PathExists(test_file()));
  UserPolicyCache cache(test_file(), false  /* wait_for_policy_fetch */);
  cache.Load();
  loop_.RunAllPending();
  PolicyMap expected;
  expected.Set(kPolicyHomepageLocation,
               Value::CreateStringValue("http://www.example.com"));
  EXPECT_TRUE(expected.Equals(mandatory_policy(cache)));
}

TEST_F(UserPolicyCacheTest, FreshPolicyOverride) {
  scoped_ptr<em::PolicyFetchResponse> policy(
      CreateHomepagePolicy("http://www.example.com",
                           base::Time::NowFromSystemTime(),
                           em::PolicyOptions::MANDATORY));
  WritePolicy(*policy);

  UserPolicyCache cache(test_file(), false  /* wait_for_policy_fetch */);
  em::PolicyFetchResponse* updated_policy =
      CreateHomepagePolicy("http://www.chromium.org",
                           base::Time::NowFromSystemTime(),
                           em::PolicyOptions::MANDATORY);
  SetPolicy(&cache, updated_policy);

  cache.Load();
  loop_.RunAllPending();
  PolicyMap expected;
  expected.Set(kPolicyHomepageLocation,
               Value::CreateStringValue("http://www.chromium.org"));
  EXPECT_TRUE(expected.Equals(mandatory_policy(cache)));
}

TEST_F(UserPolicyCacheTest, SetReady) {
  UserPolicyCache cache(test_file(), false  /* wait_for_policy_fetch */);
  cache.AddObserver(&observer_);
  scoped_ptr<em::PolicyFetchResponse> policy(
      CreateHomepagePolicy("http://www.example.com",
                           base::Time::NowFromSystemTime(),
                           em::PolicyOptions::MANDATORY));
  EXPECT_CALL(observer_, OnCacheUpdate(_)).Times(0);
  cache.SetPolicy(*policy);
  testing::Mock::VerifyAndClearExpectations(&observer_);

  // Switching the cache to ready should send a notification.
  EXPECT_CALL(observer_, OnCacheUpdate(_)).Times(1);
  SetReady(&cache);
  cache.RemoveObserver(&observer_);
}

// Test case for the temporary support for GenericNamedValues in the
// CloudPolicySettings protobuf. Can be removed when this support is no longer
// required.
TEST_F(UserPolicyCacheTest, OldStylePolicy) {
  UserPolicyCache cache(test_file(), false  /* wait_for_policy_fetch */);
  em::PolicyFetchResponse* policy = new em::PolicyFetchResponse();
  em::PolicyData signed_response;
  em::LegacyChromeSettingsProto settings;
  em::GenericNamedValue* named_value = settings.add_named_value();
  named_value->set_name("HomepageLocation");
  em::GenericValue* value_container = named_value->mutable_value();
  value_container->set_value_type(em::GenericValue::VALUE_TYPE_STRING);
  value_container->set_string_value("http://www.example.com");
  EXPECT_TRUE(
      settings.SerializeToString(signed_response.mutable_policy_value()));
  base::TimeDelta timestamp =
      base::Time::NowFromSystemTime() - base::Time::UnixEpoch();
  signed_response.set_timestamp(timestamp.InMilliseconds());
  EXPECT_TRUE(
      signed_response.SerializeToString(policy->mutable_policy_data()));

  SetPolicy(&cache, policy);
  PolicyMap expected;
  expected.Set(kPolicyHomepageLocation,
               Value::CreateStringValue("http://www.example.com"));
  PolicyMap empty;
  EXPECT_TRUE(expected.Equals(mandatory_policy(cache)));
  EXPECT_TRUE(empty.Equals(recommended_policy(cache)));
  // If new-style policy comes in, it should override old-style policy.
  policy = CreateHomepagePolicy("http://www.example.com",
                                base::Time::NowFromSystemTime(),
                                em::PolicyOptions::RECOMMENDED);
  SetPolicy(&cache, policy);
  EXPECT_TRUE(expected.Equals(recommended_policy(cache)));
  EXPECT_TRUE(empty.Equals(mandatory_policy(cache)));
}

TEST_F(UserPolicyCacheTest, CheckReadyNoWaiting) {
  UserPolicyCache cache(test_file(), false  /* wait_for_policy_fetch */);
  EXPECT_FALSE(cache.IsReady());
  cache.Load();
  loop_.RunAllPending();
  EXPECT_TRUE(cache.IsReady());
}

TEST_F(UserPolicyCacheTest, CheckReadyWaitForFetch) {
  UserPolicyCache cache(test_file(), true  /* wait_for_policy_fetch */);
  EXPECT_FALSE(cache.IsReady());
  cache.Load();
  loop_.RunAllPending();
  EXPECT_FALSE(cache.IsReady());
  cache.SetFetchingDone();
  EXPECT_TRUE(cache.IsReady());
}

TEST_F(UserPolicyCacheTest, CheckReadyWaitForDisk) {
  UserPolicyCache cache(test_file(), true  /* wait_for_policy_fetch */);
  EXPECT_FALSE(cache.IsReady());
  cache.SetFetchingDone();
  EXPECT_FALSE(cache.IsReady());
  cache.Load();
  loop_.RunAllPending();
  EXPECT_TRUE(cache.IsReady());
}

}  // namespace policy
