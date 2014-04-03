// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/profile_pref_store_manager.h"

#include <vector>

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/persistent_pref_store.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_factory.h"
#include "base/prefs/pref_store.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_hash_filter.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FirstEqualsPredicate {
 public:
  explicit FirstEqualsPredicate(const std::string& expected)
      : expected_(expected) {}
  bool operator()(const std::pair<std::string, base::Value*>& pair) {
    return pair.first == expected_;
  }

 private:
  const std::string expected_;
};

// Observes changes to the PrefStore and verifies that only registered prefs are
// written.
class RegistryVerifier : public PrefStore::Observer {
 public:
  explicit RegistryVerifier(PrefRegistry* pref_registry)
      : pref_registry_(pref_registry) {}

  // PrefStore::Observer implementation
  virtual void OnPrefValueChanged(const std::string& key) OVERRIDE {
    EXPECT_TRUE(pref_registry_->end() !=
                std::find_if(pref_registry_->begin(),
                             pref_registry_->end(),
                             FirstEqualsPredicate(key)))
        << "Unregistered key " << key << " was changed.";
  }

  virtual void OnInitializationCompleted(bool succeeded) OVERRIDE {}

 private:
  scoped_refptr<PrefRegistry> pref_registry_;
};

const char kUnprotectedAtomic[] = "unprotected_atomic";
const char kTrackedAtomic[] = "tracked_atomic";
const char kProtectedAtomic[] = "protected_atomic";

const char kFoobar[] = "FOOBAR";
const char kBarfoo[] = "BARFOO";
const char kHelloWorld[] = "HELLOWORLD";
const char kGoodbyeWorld[] = "GOODBYEWORLD";

const PrefHashFilter::TrackedPreferenceMetadata kConfiguration[] = {
    {0u, kTrackedAtomic, PrefHashFilter::NO_ENFORCEMENT,
     PrefHashFilter::TRACKING_STRATEGY_ATOMIC},
    {1u, kProtectedAtomic, PrefHashFilter::ENFORCE_ON_LOAD,
     PrefHashFilter::TRACKING_STRATEGY_ATOMIC}};

const size_t kExtraReportingId = 2u;
const size_t kReportingIdCount = 3u;

}  // namespace

class ProfilePrefStoreManagerTest : public testing::Test {
 public:
  ProfilePrefStoreManagerTest()
      : configuration_(kConfiguration,
                       kConfiguration + arraysize(kConfiguration)),
        profile_pref_registry_(new user_prefs::PrefRegistrySyncable),
        registry_verifier_(profile_pref_registry_) {}

  virtual void SetUp() OVERRIDE {
    ProfilePrefStoreManager::RegisterPrefs(local_state_.registry());
    ProfilePrefStoreManager::RegisterProfilePrefs(profile_pref_registry_);
    for (const PrefHashFilter::TrackedPreferenceMetadata* it = kConfiguration;
         it != kConfiguration + arraysize(kConfiguration);
         ++it) {
      if (it->strategy == PrefHashFilter::TRACKING_STRATEGY_ATOMIC) {
        profile_pref_registry_->RegisterStringPref(
            it->name, "", user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
      } else {
        profile_pref_registry_->RegisterDictionaryPref(
            it->name, user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
      }
    }
    profile_pref_registry_->RegisterStringPref(
        kUnprotectedAtomic,
        std::string(),
        user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());
    ReloadConfiguration();
  }

  void ReloadConfiguration() {
    manager_.reset(new ProfilePrefStoreManager(profile_dir_.path(),
                                               configuration_,
                                               kReportingIdCount,
                                               "seed",
                                               "device_id",
                                               &local_state_));
  }

  virtual void TearDown() OVERRIDE { DestroyPrefStore(); }

 protected:
  bool WasResetRecorded() {
    base::PrefServiceFactory pref_service_factory;
    pref_service_factory.set_user_prefs(pref_store_);

    scoped_ptr<PrefService> pref_service(
        pref_service_factory.Create(profile_pref_registry_));

    return !ProfilePrefStoreManager::GetResetTime(pref_service.get()).is_null();
  }

  void InitializePrefs() {
    // According to the implementation of ProfilePrefStoreManager, this is
    // actually a SegregatedPrefStore backed by two underlying pref stores.
    scoped_refptr<PersistentPrefStore> pref_store =
        manager_->CreateProfilePrefStore(
            main_message_loop_.message_loop_proxy());
    InitializePrefStore(pref_store);
    pref_store = NULL;
    base::RunLoop().RunUntilIdle();
  }

  void DestroyPrefStore() {
    if (pref_store_) {
      // Force everything to be written to disk, triggering the PrefHashFilter
      // while our RegistryVerifier is watching.
      pref_store_->CommitPendingWrite();
      base::RunLoop().RunUntilIdle();

      pref_store_->RemoveObserver(&registry_verifier_);
      pref_store_ = NULL;
      // Nothing should have to happen on the background threads, but just in
      // case...
      base::RunLoop().RunUntilIdle();
    }
  }

  void InitializeDeprecatedCombinedProfilePrefStore() {
    scoped_refptr<PersistentPrefStore> pref_store =
        manager_->CreateDeprecatedCombinedProfilePrefStore(
            main_message_loop_.message_loop_proxy());
    InitializePrefStore(pref_store);
    pref_store = NULL;
    base::RunLoop().RunUntilIdle();
  }

  void InitializePrefStore(PersistentPrefStore* pref_store) {
    pref_store->AddObserver(&registry_verifier_);
    PersistentPrefStore::PrefReadError error = pref_store->ReadPrefs();
    EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NO_FILE, error);
    pref_store->SetValue(kTrackedAtomic, new base::StringValue(kFoobar));
    pref_store->SetValue(kProtectedAtomic, new base::StringValue(kHelloWorld));
    pref_store->SetValue(kUnprotectedAtomic, new base::StringValue(kFoobar));
    pref_store->RemoveObserver(&registry_verifier_);
    pref_store->CommitPendingWrite();
    base::RunLoop().RunUntilIdle();
  }

  void LoadExistingPrefs() {
    DestroyPrefStore();
    pref_store_ = manager_->CreateProfilePrefStore(
        main_message_loop_.message_loop_proxy());
    pref_store_->AddObserver(&registry_verifier_);
    pref_store_->ReadPrefs();
  }

  void ReplaceStringInPrefs(const std::string& find,
                            const std::string& replace) {
    base::FileEnumerator file_enum(
        profile_dir_.path(), true, base::FileEnumerator::FILES);

    for (base::FilePath path = file_enum.Next(); !path.empty();
         path = file_enum.Next()) {
      // Tamper with the file's contents
      std::string contents;
      EXPECT_TRUE(base::ReadFileToString(path, &contents));
      ReplaceSubstringsAfterOffset(&contents, 0u, find, replace);
      EXPECT_EQ(static_cast<int>(contents.length()),
                base::WriteFile(path, contents.c_str(), contents.length()));
    }
  }

  void ExpectStringValueEquals(const std::string& name,
                               const std::string& expected) {
    const base::Value* value = NULL;
    std::string as_string;
    if (!pref_store_->GetValue(name, &value)) {
      ADD_FAILURE() << name << " is not a defined value.";
    } else if (!value->GetAsString(&as_string)) {
      ADD_FAILURE() << name << " could not be coerced to a string.";
    } else {
      EXPECT_EQ(expected, as_string);
    }
  }

  base::MessageLoop main_message_loop_;
  std::vector<PrefHashFilter::TrackedPreferenceMetadata> configuration_;
  base::ScopedTempDir profile_dir_;
  TestingPrefServiceSimple local_state_;
  scoped_refptr<user_prefs::PrefRegistrySyncable> profile_pref_registry_;
  RegistryVerifier registry_verifier_;
  scoped_ptr<ProfilePrefStoreManager> manager_;
  scoped_refptr<PersistentPrefStore> pref_store_;
};

TEST_F(ProfilePrefStoreManagerTest, StoreValues) {
  InitializePrefs();

  LoadExistingPrefs();

  ExpectStringValueEquals(kTrackedAtomic, kFoobar);
  ExpectStringValueEquals(kProtectedAtomic, kHelloWorld);
  EXPECT_FALSE(WasResetRecorded());
}

TEST_F(ProfilePrefStoreManagerTest, GetPrefFilePathFromProfilePath) {
  base::FilePath pref_file_path =
      ProfilePrefStoreManager::GetPrefFilePathFromProfilePath(
          profile_dir_.path());

  EXPECT_FALSE(base::PathExists(pref_file_path));

  InitializePrefs();

  EXPECT_TRUE(base::PathExists(pref_file_path));
}

TEST_F(ProfilePrefStoreManagerTest, ProtectValues) {
  InitializePrefs();

  ReplaceStringInPrefs(kFoobar, kBarfoo);
  ReplaceStringInPrefs(kHelloWorld, kGoodbyeWorld);

  LoadExistingPrefs();

  // kTrackedAtomic is unprotected and thus will be loaded as it appears on
  // disk.
  ExpectStringValueEquals(kTrackedAtomic, kBarfoo);

  // If preference tracking is supported, the tampered value of kProtectedAtomic
  // will be discarded at load time, leaving this preference undefined.
  EXPECT_NE(ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            pref_store_->GetValue(kProtectedAtomic, NULL));
  EXPECT_EQ(ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            WasResetRecorded());
}

TEST_F(ProfilePrefStoreManagerTest, ResetPrefHashStore) {
  InitializePrefs();

  manager_->ResetPrefHashStore();

  LoadExistingPrefs();

  // kTrackedAtomic is loaded as it appears on disk.
  ExpectStringValueEquals(kTrackedAtomic, kFoobar);
  // If preference tracking is supported, the tampered value of kProtectedAtomic
  // will be discarded at load time, leaving this preference undefined.
  EXPECT_NE(ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            pref_store_->GetValue(kProtectedAtomic, NULL));
  EXPECT_EQ(ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            WasResetRecorded());
}

TEST_F(ProfilePrefStoreManagerTest, ResetAllPrefHashStores) {
  InitializePrefs();

  ProfilePrefStoreManager::ResetAllPrefHashStores(&local_state_);

  LoadExistingPrefs();

  // kTrackedAtomic is loaded as it appears on disk.
  ExpectStringValueEquals(kTrackedAtomic, kFoobar);
  // If preference tracking is supported, kProtectedAtomic will be undefined
  // because the value was discarded due to loss of the hash store contents.
  EXPECT_NE(ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            pref_store_->GetValue(kProtectedAtomic, NULL));
  EXPECT_EQ(ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            WasResetRecorded());
}

TEST_F(ProfilePrefStoreManagerTest, MigrateFromOneFile) {
  InitializeDeprecatedCombinedProfilePrefStore();

  LoadExistingPrefs();

  ExpectStringValueEquals(kTrackedAtomic, kFoobar);
  ExpectStringValueEquals(kProtectedAtomic, kHelloWorld);
  EXPECT_FALSE(WasResetRecorded());
}

TEST_F(ProfilePrefStoreManagerTest, UpdateProfileHashStoreIfRequired) {
  scoped_refptr<JsonPrefStore> legacy_prefs(
      new JsonPrefStore(ProfilePrefStoreManager::GetPrefFilePathFromProfilePath(
                            profile_dir_.path()),
                        main_message_loop_.message_loop_proxy(),
                        scoped_ptr<PrefFilter>()));
  legacy_prefs->SetValue(kTrackedAtomic, new base::StringValue(kFoobar));
  legacy_prefs->SetValue(kProtectedAtomic, new base::StringValue(kHelloWorld));
  legacy_prefs = NULL;
  base::RunLoop().RunUntilIdle();

  // This is a no-op if !kPlatformSupportsPreferenceTracking.
  manager_->UpdateProfileHashStoreIfRequired(
      main_message_loop_.message_loop_proxy());
  base::RunLoop().RunUntilIdle();

  // At the moment, UpdateProfileHashStoreIfRequired will accept existing
  // values.
  LoadExistingPrefs();

  // These expectations hold whether or not tracking is supported.
  ExpectStringValueEquals(kTrackedAtomic, kFoobar);
  ExpectStringValueEquals(kProtectedAtomic, kHelloWorld);
  EXPECT_FALSE(WasResetRecorded());
}

TEST_F(ProfilePrefStoreManagerTest, InitializePrefsFromMasterPrefs) {
  scoped_ptr<base::DictionaryValue> master_prefs(
      new base::DictionaryValue);
  master_prefs->Set(kTrackedAtomic, new base::StringValue(kFoobar));
  master_prefs->Set(kProtectedAtomic, new base::StringValue(kHelloWorld));
  EXPECT_TRUE(
      manager_->InitializePrefsFromMasterPrefs(*master_prefs));

  LoadExistingPrefs();

  // Verify that InitializePrefsFromMasterPrefs correctly applied the MACs
  // necessary to authenticate these values.
  ExpectStringValueEquals(kTrackedAtomic, kFoobar);
  ExpectStringValueEquals(kProtectedAtomic, kHelloWorld);
  EXPECT_FALSE(WasResetRecorded());
}

TEST_F(ProfilePrefStoreManagerTest, UnprotectedToProtected) {
  InitializePrefs();
  LoadExistingPrefs();
  ExpectStringValueEquals(kUnprotectedAtomic, kFoobar);

  // Ensure everything is written out to disk.
  DestroyPrefStore();

  ReplaceStringInPrefs(kFoobar, kBarfoo);

  // It's unprotected, so we can load the modified value.
  LoadExistingPrefs();
  ExpectStringValueEquals(kUnprotectedAtomic, kBarfoo);

  // Now update the configuration to protect it.
  PrefHashFilter::TrackedPreferenceMetadata new_protected = {
      kExtraReportingId, kUnprotectedAtomic, PrefHashFilter::ENFORCE_ON_LOAD,
      PrefHashFilter::TRACKING_STRATEGY_ATOMIC};
  configuration_.push_back(new_protected);
  ReloadConfiguration();

  // And try loading with the new configuration.
  LoadExistingPrefs();

  // Since there was a valid super MAC we were able to extend the existing trust
  // to the newly proteted preference.
  ExpectStringValueEquals(kUnprotectedAtomic, kBarfoo);
  EXPECT_FALSE(WasResetRecorded());

  // Ensure everything is written out to disk.
  DestroyPrefStore();

  // It's protected now, so (if the platform supports it) any tampering should
  // lead to a reset.
  ReplaceStringInPrefs(kBarfoo, kFoobar);
  LoadExistingPrefs();
  EXPECT_NE(ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            pref_store_->GetValue(kUnprotectedAtomic, NULL));
  EXPECT_EQ(ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            WasResetRecorded());
}

TEST_F(ProfilePrefStoreManagerTest, UnprotectedToProtectedWithoutTrust) {
  InitializePrefs();

  // Now update the configuration to protect it.
  PrefHashFilter::TrackedPreferenceMetadata new_protected = {
      kExtraReportingId, kUnprotectedAtomic, PrefHashFilter::ENFORCE_ON_LOAD,
      PrefHashFilter::TRACKING_STRATEGY_ATOMIC};
  configuration_.push_back(new_protected);
  ReloadConfiguration();
  ProfilePrefStoreManager::ResetAllPrefHashStores(&local_state_);

  // And try loading with the new configuration.
  LoadExistingPrefs();

  // If preference tracking is supported, kUnprotectedAtomic will have been
  // discarded because new values are not accepted without a valid super MAC.
  EXPECT_NE(ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            pref_store_->GetValue(kUnprotectedAtomic, NULL));
  EXPECT_EQ(ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            WasResetRecorded());
}

// This test does not directly verify that the values are moved from one pref
// store to the other. segregated_pref_store_unittest.cc _does_ verify that
// functionality.
//
// _This_ test verifies that preference values are correctly maintained when a
// preference's protection state changes from protected to unprotected.
TEST_F(ProfilePrefStoreManagerTest, ProtectedToUnprotected) {
  InitializePrefs();
  DestroyPrefStore();

  // Unconfigure protection for kProtectedAtomic
  for (std::vector<PrefHashFilter::TrackedPreferenceMetadata>::iterator it =
           configuration_.begin();
       it != configuration_.end();
       ++it) {
    if (it->name == kProtectedAtomic) {
      configuration_.erase(it);
      break;
    }
  }
  ReloadConfiguration();

  // Reset the hash stores and then try loading the prefs.
  ProfilePrefStoreManager::ResetAllPrefHashStores(&local_state_);
  LoadExistingPrefs();

  // Verify that the value was not reset.
  ExpectStringValueEquals(kProtectedAtomic, kHelloWorld);
  EXPECT_FALSE(WasResetRecorded());

  // Accessing the value of the previously protected pref didn't trigger its
  // move to the unprotected preferences file, though the loading of the pref
  // store should still have caused the MAC store to be recalculated.
  LoadExistingPrefs();
  ExpectStringValueEquals(kProtectedAtomic, kHelloWorld);

  // Trigger the logic that migrates it back to the unprotected preferences
  // file.
  pref_store_->SetValue(kProtectedAtomic, new base::StringValue(kGoodbyeWorld));
  LoadExistingPrefs();
  ExpectStringValueEquals(kProtectedAtomic, kGoodbyeWorld);
  EXPECT_FALSE(WasResetRecorded());
}
