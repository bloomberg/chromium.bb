// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/unguessable_token.h"
#include "base/value_conversions.h"
#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/base/android/media_drm_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// These values must match the values specified for the implementation
// in media_drm_origin_id_manager.cc.
const char kMediaDrmOriginIds[] = "media.media_drm_origin_ids";
const char kExpirableToken[] = "expirable_token";
const char kAvailableOriginIds[] = "origin_ids";
constexpr size_t kExpectedPreferenceListSize = 5;
constexpr base::TimeDelta kExpirationDelta = base::TimeDelta::FromHours(24);

}  // namespace

class MediaDrmOriginIdManagerTest : public testing::Test {
 public:
  MediaDrmOriginIdManagerTest() {
    profile_ = std::make_unique<TestingProfile>();
    origin_id_manager_ =
        MediaDrmOriginIdManagerFactory::GetForProfile(profile_.get());
    origin_id_manager_->SetProvisioningResultForTesting(true);
  }

  // Call MediaDrmOriginIdManager::GetOriginId() synchronously.
  base::UnguessableToken GetOriginId() {
    base::RunLoop run_loop;
    base::UnguessableToken result;

    origin_id_manager_->GetOriginId(base::BindOnce(
        [](base::OnceClosure callback, base::UnguessableToken* result,
           bool success, const base::UnguessableToken& origin_id) {
          EXPECT_TRUE(success);
          *result = origin_id;
          std::move(callback).Run();
        },
        run_loop.QuitClosure(), &result));
    run_loop.Run();
    return result;
  }

  void PreProvision() {
    origin_id_manager_->PreProvisionIfNecessary();
    test_browser_thread_bundle_.RunUntilIdle();
  }

  std::string DisplayPref(const base::Value* value) {
    std::string output;
    JSONStringValueSerializer serializer(&output);
    EXPECT_TRUE(serializer.Serialize(*value));
    return output;
  }

  const PrefService::Preference* FindPreference(const std::string& path) const {
    return profile_->GetTestingPrefService()->FindPreference(path);
  }

  const base::DictionaryValue* GetDictionary(const std::string& path) const {
    return profile_->GetTestingPrefService()->GetDictionary(path);
  }

 protected:
  content::TestBrowserThreadBundle test_browser_thread_bundle_{
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
      base::test::ScopedTaskEnvironment::NowSource::MAIN_THREAD_MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  MediaDrmOriginIdManager* origin_id_manager_;
};

TEST_F(MediaDrmOriginIdManagerTest, Creation) {
  // Test verifies that the construction of MediaDrmOriginIdManager is
  // successful.
}

TEST_F(MediaDrmOriginIdManagerTest, OneOriginId) {
  EXPECT_TRUE(GetOriginId());
}

TEST_F(MediaDrmOriginIdManagerTest, TwoOriginIds) {
  base::UnguessableToken origin_id1 = GetOriginId();
  base::UnguessableToken origin_id2 = GetOriginId();
  EXPECT_TRUE(origin_id1);
  EXPECT_TRUE(origin_id2);
  EXPECT_NE(origin_id1, origin_id2);
}

TEST_F(MediaDrmOriginIdManagerTest, PreProvision) {
  // On devices that support per-application provisioning PreProvision() will
  // pre-provisioned several origin IDs and populate the preference. On devices
  // that don't, the list will be empty. Note that simply finding the preference
  // creates an empty one (as FindPreference() only returns NULL if the
  // preference is not registered).
  PreProvision();

  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds;
  auto* pref = FindPreference(kMediaDrmOriginIds);
  EXPECT_TRUE(pref);
  EXPECT_EQ(kMediaDrmOriginIds, pref->name());
  EXPECT_EQ(base::Value::Type::DICTIONARY, pref->GetType());

  auto* dict = pref->GetValue();
  EXPECT_TRUE(dict->is_dict());
  DVLOG(1) << DisplayPref(pref->GetValue());

  if (media::MediaDrmBridge::IsPerApplicationProvisioningSupported()) {
    DVLOG(1) << "Per-application provisioning is supported.";

    // PreProvision() should have pre-provisioned |kExpectedPreferenceListSize|
    // origin IDs.
    auto* list = dict->FindKey(kAvailableOriginIds);
    EXPECT_TRUE(list->is_list());
    EXPECT_EQ(list->GetList().size(), kExpectedPreferenceListSize);
  } else {
    DVLOG(1) << "Per-application provisioning is NOT supported.";

    // No pre-provisioned origin IDs should exist. In fact, the dictionary
    // should not have any entries.
    auto* list = dict->FindKey(kAvailableOriginIds);
    EXPECT_FALSE(list);
    EXPECT_EQ(dict->DictSize(), 0u);
  }
}

TEST_F(MediaDrmOriginIdManagerTest, GetOriginIdCreatesList) {
  // After fetching an origin ID the code should pre-provision more origins
  // and fill up the list.
  GetOriginId();
  test_browser_thread_bundle_.RunUntilIdle();

  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds;
  auto* pref = FindPreference(kMediaDrmOriginIds);
  EXPECT_TRUE(pref);

  auto* dict = pref->GetValue();
  EXPECT_TRUE(dict->is_dict());
  DVLOG(1) << DisplayPref(pref->GetValue());

  auto* list = dict->FindKey(kAvailableOriginIds);
  EXPECT_TRUE(list->is_list());
  EXPECT_EQ(list->GetList().size(), kExpectedPreferenceListSize);
}

TEST_F(MediaDrmOriginIdManagerTest, OriginIdNotInList) {
  base::UnguessableToken origin_id;

  // After fetching one origin ID MediaDrmOriginIdManager will create the list
  // of pre-provisioned origin IDs (asynchronously). It doesn't matter if the
  // device supports per-application provisioning or not.
  origin_id = GetOriginId();
  test_browser_thread_bundle_.RunUntilIdle();

  // Check that the preference does not contain |origin_id|.
  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds;
  auto* dict = GetDictionary(kMediaDrmOriginIds);
  auto* list = dict->FindKey(kAvailableOriginIds);
  EXPECT_FALSE(
      ContainsValue(list->GetList(), CreateUnguessableTokenValue(origin_id)));
}

TEST_F(MediaDrmOriginIdManagerTest, ProvisioningFail) {
  // TODO(crbug.com/917527): Currently the code returns an origin ID even if
  // provisioning fails. Update this once it returns an empty origin ID when
  // pre-provisioning fails.
  origin_id_manager_->SetProvisioningResultForTesting(false);
  EXPECT_TRUE(GetOriginId());

  // After failure the preference should contain |kExpireableToken| only if
  // per-application provisioning is not supported.
  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds;
  auto* dict = GetDictionary(kMediaDrmOriginIds);
  DVLOG(1) << DisplayPref(dict);
  if (media::MediaDrmBridge::IsPerApplicationProvisioningSupported()) {
    EXPECT_FALSE(dict->FindKey(kExpirableToken));
  } else {
    EXPECT_TRUE(dict->FindKey(kExpirableToken));
  }
}

TEST_F(MediaDrmOriginIdManagerTest, ProvisioningSuccessAfterFail) {
  // TODO(crbug.com/917527): Currently the code returns an origin ID even if
  // provisioning fails. Update this once it returns an empty origin ID when
  // pre-provisioning fails.
  origin_id_manager_->SetProvisioningResultForTesting(false);
  EXPECT_TRUE(GetOriginId());

  origin_id_manager_->SetProvisioningResultForTesting(true);
  EXPECT_TRUE(GetOriginId());

  // Let pre-provisioning of other origin IDs finish.
  test_browser_thread_bundle_.RunUntilIdle();

  // After success the preference should not contain |kExpireableToken|.
  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds;
  auto* dict = GetDictionary(kMediaDrmOriginIds);
  DVLOG(1) << DisplayPref(dict);
  EXPECT_FALSE(dict->FindKey(kExpirableToken));
}

TEST_F(MediaDrmOriginIdManagerTest, ProvisioningAfterExpiration) {
  // TODO(crbug.com/917527): Currently the code returns an origin ID even if
  // provisioning fails. Update this once it returns an empty origin ID when
  // pre-provisioning fails.
  DVLOG(1) << "Current time: " << base::Time::Now();
  origin_id_manager_->SetProvisioningResultForTesting(false);
  EXPECT_TRUE(GetOriginId());
  test_browser_thread_bundle_.RunUntilIdle();

  // Check that |kAvailableOriginIds| in the preference is empty.
  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds;
  auto* dict = GetDictionary(kMediaDrmOriginIds);
  DVLOG(1) << DisplayPref(dict);
  EXPECT_FALSE(dict->FindKey(kAvailableOriginIds));

  // Check that |kExpirableToken| is only set if per-application provisioning is
  // not supported.
  EXPECT_TRUE(media::MediaDrmBridge::IsPerApplicationProvisioningSupported() ||
              dict->FindKey(kExpirableToken));

  // Advance clock by |kExpirationDelta| (plus one minute) and attempt to
  // pre-provision more origin Ids.
  DVLOG(1) << "Advancing Time";
  test_browser_thread_bundle_.FastForwardBy(kExpirationDelta);
  test_browser_thread_bundle_.FastForwardBy(base::TimeDelta::FromMinutes(1));
  DVLOG(1) << "Adjusted time: " << base::Time::Now();
  origin_id_manager_->SetProvisioningResultForTesting(true);
  PreProvision();
  test_browser_thread_bundle_.RunUntilIdle();

  // Look at the preference again.
  DVLOG(1) << "Checking preference " << kMediaDrmOriginIds << " again";
  dict = GetDictionary(kMediaDrmOriginIds);
  DVLOG(1) << DisplayPref(dict);
  auto* list = dict->FindKey(kAvailableOriginIds);

  if (media::MediaDrmBridge::IsPerApplicationProvisioningSupported()) {
    // If per-application provisioning is supported, it's OK to attempt
    // to pre-provision origin IDs any time.
    DVLOG(1) << "Per-application provisioning is supported.";
    EXPECT_EQ(list->GetList().size(), kExpectedPreferenceListSize);
  } else {
    // Per-application provisioning is not supported, so attempting to
    // pre-provision origin IDs after |kExpirationDelta| should not do anything.
    // As well, |kExpirableToken| should be removed.
    DVLOG(1) << "Per-application provisioning is NOT supported.";
    EXPECT_FALSE(list);
    EXPECT_FALSE(dict->FindKey(kExpirableToken));
  }
}

TEST_F(MediaDrmOriginIdManagerTest, Incognito) {
  // No MediaDrmOriginIdManager should be created for an incognito profile.
  auto* incognito_profile = profile_->GetOffTheRecordProfile();
  EXPECT_FALSE(
      MediaDrmOriginIdManagerFactory::GetForProfile(incognito_profile));
}
