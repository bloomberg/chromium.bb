// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/profile_pref_store_manager.h"

#include <vector>

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/persistent_pref_store.h"
#include "base/prefs/pref_service.h"
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

const char kTrackedAtomic[] = "tracked_atomic";
const char kProtectedAtomic[] = "protected_atomic";
const char kProtectedSplit[] = "protected_split";

const char kFoobar[] = "FOOBAR";
const char kBarfoo[] = "BARFOO";
const char kHelloWorld[] = "HELLOWORLD";
const char kGoodbyeWorld[] = "GOODBYEWORLD";

const PrefHashFilter::TrackedPreferenceMetadata kConfiguration[] = {
    {0, kTrackedAtomic, PrefHashFilter::NO_ENFORCEMENT,
     PrefHashFilter::TRACKING_STRATEGY_ATOMIC},
    {1, kProtectedAtomic, PrefHashFilter::ENFORCE_ON_LOAD,
     PrefHashFilter::TRACKING_STRATEGY_ATOMIC},
    {2, kProtectedSplit, PrefHashFilter::ENFORCE_ON_LOAD,
     PrefHashFilter::TRACKING_STRATEGY_SPLIT}};

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
    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());

    manager_.reset(new ProfilePrefStoreManager(profile_dir_.path(),
                                               configuration_,
                                               configuration_.size(),
                                               "seed",
                                               "device_id",
                                               &local_state_));
  }

  virtual void TearDown() OVERRIDE {
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

 protected:
  void InitializePrefs() {
    scoped_refptr<PersistentPrefStore> pref_store =
        manager_->CreateProfilePrefStore(
            main_message_loop_.message_loop_proxy());
    pref_store->AddObserver(&registry_verifier_);
    PersistentPrefStore::PrefReadError error = pref_store->ReadPrefs();
    ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NO_FILE, error);
    pref_store->SetValue(kTrackedAtomic, new base::StringValue(kFoobar));
    pref_store->SetValue(kProtectedAtomic, new base::StringValue(kHelloWorld));
    pref_store->RemoveObserver(&registry_verifier_);
    pref_store = NULL;
    base::RunLoop().RunUntilIdle();
  }

  void LoadExistingPrefs() {
    pref_store_ = manager_->CreateProfilePrefStore(
        main_message_loop_.message_loop_proxy());
    pref_store_->AddObserver(&registry_verifier_);
    EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE,
              pref_store_->ReadPrefs());
  }

  void ReplaceStringInPrefs(const std::string& find,
                            const std::string& replace) {
    // Tamper with the file's contents
    base::FilePath pref_file_path =
        ProfilePrefStoreManager::GetPrefFilePathFromProfilePath(
            profile_dir_.path());
    std::string pref_file_contents;
    EXPECT_TRUE(base::ReadFileToString(pref_file_path, &pref_file_contents));
    ReplaceSubstringsAfterOffset(&pref_file_contents, 0u, find, replace);
    EXPECT_EQ(static_cast<int>(pref_file_contents.length()),
              base::WriteFile(pref_file_path,
                              pref_file_contents.c_str(),
                              pref_file_contents.length()));
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
}

TEST_F(ProfilePrefStoreManagerTest, GetPrefFilePathFromProfilePath) {
  base::FilePath pref_file_path =
      ProfilePrefStoreManager::GetPrefFilePathFromProfilePath(
          profile_dir_.path());

  ASSERT_FALSE(base::PathExists(pref_file_path));

  InitializePrefs();

  ASSERT_TRUE(base::PathExists(pref_file_path));
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
  EXPECT_EQ(!ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            pref_store_->GetValue(kProtectedAtomic, NULL));
}

TEST_F(ProfilePrefStoreManagerTest, ResetPrefHashStore) {
  InitializePrefs();

  manager_->ResetPrefHashStore();

  LoadExistingPrefs();

  // kTrackedAtomic is loaded as it appears on disk.
  ExpectStringValueEquals(kTrackedAtomic, kFoobar);
  // If preference tracking is supported, the tampered value of kProtectedAtomic
  // will be discarded at load time, leaving this preference undefined.
  EXPECT_EQ(!ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            pref_store_->GetValue(kProtectedAtomic, NULL));
}

TEST_F(ProfilePrefStoreManagerTest, ResetAllPrefHashStores) {
  InitializePrefs();

  ProfilePrefStoreManager::ResetAllPrefHashStores(&local_state_);

  LoadExistingPrefs();

  // kTrackedAtomic is loaded as it appears on disk.
  ExpectStringValueEquals(kTrackedAtomic, kFoobar);
  // If preference tracking is supported, kProtectedAtomic will be undefined
  // because the value was discarded due to loss of the hash store contents.
  EXPECT_EQ(!ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            pref_store_->GetValue(kProtectedAtomic, NULL));
}

TEST_F(ProfilePrefStoreManagerTest, UpdateProfileHashStoreIfRequired) {
  InitializePrefs();

  manager_->ResetPrefHashStore();

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
}

TEST_F(ProfilePrefStoreManagerTest, InitializePrefsFromMasterPrefs) {
  scoped_ptr<base::DictionaryValue> master_prefs(
      new base::DictionaryValue);
  master_prefs->Set(kTrackedAtomic, new base::StringValue(kFoobar));
  master_prefs->Set(kProtectedAtomic, new base::StringValue(kHelloWorld));
  ASSERT_TRUE(
      manager_->InitializePrefsFromMasterPrefs(*master_prefs));

  LoadExistingPrefs();

  // Verify that InitializePrefsFromMasterPrefs correctly applied the MACs
  // necessary to authenticate these values.
  ExpectStringValueEquals(kTrackedAtomic, kFoobar);
  ExpectStringValueEquals(kProtectedAtomic, kHelloWorld);
}
