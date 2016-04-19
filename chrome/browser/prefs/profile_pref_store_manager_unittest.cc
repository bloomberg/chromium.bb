// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/profile_pref_store_manager.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/pref_store.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_prefs/tracked/mock_validation_delegate.h"
#include "components/user_prefs/tracked/pref_hash_filter.h"
#include "components/user_prefs/tracked/pref_names.h"
#include "components/user_prefs/tracked/pref_service_hash_store_contents.h"
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
  void OnPrefValueChanged(const std::string& key) override {
    EXPECT_TRUE(pref_registry_->end() !=
                std::find_if(pref_registry_->begin(),
                             pref_registry_->end(),
                             FirstEqualsPredicate(key)))
        << "Unregistered key " << key << " was changed.";
  }

  void OnInitializationCompleted(bool succeeded) override {}

 private:
  scoped_refptr<PrefRegistry> pref_registry_;
};

const char kUnprotectedPref[] = "unprotected_pref";
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
        registry_verifier_(profile_pref_registry_.get()),
        seed_("seed"),
        reset_recorded_(false) {}

  void SetUp() override {
    ProfilePrefStoreManager::RegisterPrefs(local_state_.registry());
    ProfilePrefStoreManager::RegisterProfilePrefs(profile_pref_registry_.get());
    for (const PrefHashFilter::TrackedPreferenceMetadata* it = kConfiguration;
         it != kConfiguration + arraysize(kConfiguration);
         ++it) {
      if (it->strategy == PrefHashFilter::TRACKING_STRATEGY_ATOMIC) {
        profile_pref_registry_->RegisterStringPref(it->name, std::string());
      } else {
        profile_pref_registry_->RegisterDictionaryPref(it->name);
      }
    }
    profile_pref_registry_->RegisterStringPref(kUnprotectedPref, std::string());

    // As in chrome_pref_service_factory.cc, kPreferencesResetTime needs to be
    // declared as protected in order to be read from the proper store by the
    // SegregatedPrefStore. Only declare it after configured prefs have been
    // registered above for this test as kPreferenceResetTime is already
    // registered in ProfilePrefStoreManager::RegisterProfilePrefs.
    PrefHashFilter::TrackedPreferenceMetadata pref_reset_time_config =
        {configuration_.rbegin()->reporting_id + 1,
         user_prefs::kPreferenceResetTime,
         PrefHashFilter::ENFORCE_ON_LOAD,
         PrefHashFilter::TRACKING_STRATEGY_ATOMIC};
    configuration_.push_back(pref_reset_time_config);

    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());
    ReloadConfiguration();
  }

  void ReloadConfiguration() {
    manager_.reset(new ProfilePrefStoreManager(profile_dir_.path(),
                                               configuration_,
                                               kReportingIdCount,
                                               seed_,
                                               "device_id",
                                               &local_state_));
  }

  void TearDown() override { DestroyPrefStore(); }

 protected:
  // Verifies whether a reset was reported via the RecordReset() hook. Also
  // verifies that GetResetTime() was set (or not) accordingly.
  void VerifyResetRecorded(bool reset_expected) {
    EXPECT_EQ(reset_expected, reset_recorded_);

    PrefServiceFactory pref_service_factory;
    pref_service_factory.set_user_prefs(pref_store_);

    std::unique_ptr<PrefService> pref_service(
        pref_service_factory.Create(profile_pref_registry_.get()));

    EXPECT_EQ(
        reset_expected,
        !ProfilePrefStoreManager::GetResetTime(pref_service.get()).is_null());
  }

  void ClearResetRecorded() {
    reset_recorded_ = false;

    PrefServiceFactory pref_service_factory;
    pref_service_factory.set_user_prefs(pref_store_);

    std::unique_ptr<PrefService> pref_service(
        pref_service_factory.Create(profile_pref_registry_.get()));

    ProfilePrefStoreManager::ClearResetTime(pref_service.get());
  }

  void InitializePrefs() {
    // According to the implementation of ProfilePrefStoreManager, this is
    // actually a SegregatedPrefStore backed by two underlying pref stores.
    scoped_refptr<PersistentPrefStore> pref_store =
        manager_->CreateProfilePrefStore(
            main_message_loop_.task_runner(),
            base::Bind(&ProfilePrefStoreManagerTest::RecordReset,
                       base::Unretained(this)),
            &mock_validation_delegate_);
    InitializePrefStore(pref_store.get());
    pref_store = NULL;
    base::RunLoop().RunUntilIdle();
  }

  void DestroyPrefStore() {
    if (pref_store_.get()) {
      ClearResetRecorded();
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
            main_message_loop_.task_runner());
    InitializePrefStore(pref_store.get());
    pref_store = NULL;
    base::RunLoop().RunUntilIdle();
  }

  void InitializePrefStore(PersistentPrefStore* pref_store) {
    pref_store->AddObserver(&registry_verifier_);
    PersistentPrefStore::PrefReadError error = pref_store->ReadPrefs();
    EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NO_FILE, error);
    pref_store->SetValue(kTrackedAtomic,
                         base::WrapUnique(new base::StringValue(kFoobar)),
                         WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    pref_store->SetValue(kProtectedAtomic,
                         base::WrapUnique(new base::StringValue(kHelloWorld)),
                         WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    pref_store->SetValue(kUnprotectedPref,
                         base::WrapUnique(new base::StringValue(kFoobar)),
                         WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    pref_store->RemoveObserver(&registry_verifier_);
    pref_store->CommitPendingWrite();
    base::RunLoop().RunUntilIdle();
  }

  void LoadExistingPrefs() {
    DestroyPrefStore();
    pref_store_ = manager_->CreateProfilePrefStore(
        main_message_loop_.task_runner(),
        base::Bind(&ProfilePrefStoreManagerTest::RecordReset,
                   base::Unretained(this)),
        NULL);
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
      base::ReplaceSubstringsAfterOffset(&contents, 0u, find, replace);
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

  void ExpectValidationObserved(const std::string& pref_path) {
    // No validations are expected for platforms that do not support tracking.
    if (!ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking)
      return;
    if (!mock_validation_delegate_.GetEventForPath(pref_path))
      ADD_FAILURE() << "No validation observed for preference: " << pref_path;
  }

  base::MessageLoop main_message_loop_;
  std::vector<PrefHashFilter::TrackedPreferenceMetadata> configuration_;
  base::ScopedTempDir profile_dir_;
  TestingPrefServiceSimple local_state_;
  scoped_refptr<user_prefs::PrefRegistrySyncable> profile_pref_registry_;
  RegistryVerifier registry_verifier_;
  MockValidationDelegate mock_validation_delegate_;
  std::unique_ptr<ProfilePrefStoreManager> manager_;
  scoped_refptr<PersistentPrefStore> pref_store_;

  std::string seed_;

 private:
  void RecordReset() {
    // As-is |reset_recorded_| is only designed to remember a single reset, make
    // sure none was previously recorded (or that ClearResetRecorded() was
    // called).
    EXPECT_FALSE(reset_recorded_);
    reset_recorded_ = true;
  }

  bool reset_recorded_;
};

TEST_F(ProfilePrefStoreManagerTest, StoreValues) {
  InitializePrefs();

  LoadExistingPrefs();

  ExpectStringValueEquals(kTrackedAtomic, kFoobar);
  ExpectStringValueEquals(kProtectedAtomic, kHelloWorld);
  VerifyResetRecorded(false);
  ExpectValidationObserved(kTrackedAtomic);
  ExpectValidationObserved(kProtectedAtomic);
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
  VerifyResetRecorded(
      ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking);

  ExpectValidationObserved(kTrackedAtomic);
  ExpectValidationObserved(kProtectedAtomic);
}

TEST_F(ProfilePrefStoreManagerTest, MigrateFromOneFile) {
  InitializeDeprecatedCombinedProfilePrefStore();

  // The deprecated model stores hashes in local state (on supported
  // platforms)..
  ASSERT_EQ(
      ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
      local_state_.GetUserPrefValue(
          PrefServiceHashStoreContents::kProfilePreferenceHashes) != NULL);

  LoadExistingPrefs();

  // After a first migration, the hashes were copied to the two user preference
  // files but were not cleaned.
  ASSERT_EQ(
      ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
      local_state_.GetUserPrefValue(
          PrefServiceHashStoreContents::kProfilePreferenceHashes) != NULL);

  ExpectStringValueEquals(kTrackedAtomic, kFoobar);
  ExpectStringValueEquals(kProtectedAtomic, kHelloWorld);
  VerifyResetRecorded(false);

  LoadExistingPrefs();

  // In a subsequent launch, the local state hash store should be reset.
  ASSERT_FALSE(local_state_.GetUserPrefValue(
      PrefServiceHashStoreContents::kProfilePreferenceHashes));

  ExpectStringValueEquals(kTrackedAtomic, kFoobar);
  ExpectStringValueEquals(kProtectedAtomic, kHelloWorld);
  VerifyResetRecorded(false);
}

TEST_F(ProfilePrefStoreManagerTest, MigrateWithTampering) {
  InitializeDeprecatedCombinedProfilePrefStore();

  ReplaceStringInPrefs(kFoobar, kBarfoo);
  ReplaceStringInPrefs(kHelloWorld, kGoodbyeWorld);

  // The deprecated model stores hashes in local state (on supported
  // platforms)..
  ASSERT_EQ(
      ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
      local_state_.GetUserPrefValue(
          PrefServiceHashStoreContents::kProfilePreferenceHashes) != NULL);

  LoadExistingPrefs();

  // After a first migration, the hashes were copied to the two user preference
  // files but were not cleaned.
  ASSERT_EQ(
      ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
      local_state_.GetUserPrefValue(
          PrefServiceHashStoreContents::kProfilePreferenceHashes) != NULL);

  // kTrackedAtomic is unprotected and thus will be loaded as it appears on
  // disk.
  ExpectStringValueEquals(kTrackedAtomic, kBarfoo);

  // If preference tracking is supported, the tampered value of kProtectedAtomic
  // will be discarded at load time, leaving this preference undefined.
  EXPECT_NE(ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            pref_store_->GetValue(kProtectedAtomic, NULL));
  VerifyResetRecorded(
      ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking);

  LoadExistingPrefs();

  // In a subsequent launch, the local state hash store would be reset.
  ASSERT_FALSE(local_state_.GetUserPrefValue(
      PrefServiceHashStoreContents::kProfilePreferenceHashes));

  ExpectStringValueEquals(kTrackedAtomic, kBarfoo);
  VerifyResetRecorded(false);
}

TEST_F(ProfilePrefStoreManagerTest, InitializePrefsFromMasterPrefs) {
  base::DictionaryValue master_prefs;
  master_prefs.Set(kTrackedAtomic, new base::StringValue(kFoobar));
  master_prefs.Set(kProtectedAtomic, new base::StringValue(kHelloWorld));
  EXPECT_TRUE(manager_->InitializePrefsFromMasterPrefs(master_prefs));

  LoadExistingPrefs();

  // Verify that InitializePrefsFromMasterPrefs correctly applied the MACs
  // necessary to authenticate these values.
  ExpectStringValueEquals(kTrackedAtomic, kFoobar);
  ExpectStringValueEquals(kProtectedAtomic, kHelloWorld);
  VerifyResetRecorded(false);
}

TEST_F(ProfilePrefStoreManagerTest, UnprotectedToProtected) {
  InitializePrefs();

  ExpectValidationObserved(kTrackedAtomic);
  ExpectValidationObserved(kProtectedAtomic);

  LoadExistingPrefs();
  ExpectStringValueEquals(kUnprotectedPref, kFoobar);

  // Ensure everything is written out to disk.
  DestroyPrefStore();

  ReplaceStringInPrefs(kFoobar, kBarfoo);

  // It's unprotected, so we can load the modified value.
  LoadExistingPrefs();
  ExpectStringValueEquals(kUnprotectedPref, kBarfoo);

  // Now update the configuration to protect it.
  PrefHashFilter::TrackedPreferenceMetadata new_protected = {
      kExtraReportingId, kUnprotectedPref, PrefHashFilter::ENFORCE_ON_LOAD,
      PrefHashFilter::TRACKING_STRATEGY_ATOMIC};
  configuration_.push_back(new_protected);
  ReloadConfiguration();

  // And try loading with the new configuration.
  LoadExistingPrefs();

  // Since there was a valid super MAC we were able to extend the existing trust
  // to the newly protected preference.
  ExpectStringValueEquals(kUnprotectedPref, kBarfoo);
  VerifyResetRecorded(false);

  // Ensure everything is written out to disk.
  DestroyPrefStore();

  // It's protected now, so (if the platform supports it) any tampering should
  // lead to a reset.
  ReplaceStringInPrefs(kBarfoo, kFoobar);
  LoadExistingPrefs();
  EXPECT_NE(ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            pref_store_->GetValue(kUnprotectedPref, NULL));
  VerifyResetRecorded(
      ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking);
}

TEST_F(ProfilePrefStoreManagerTest, NewPrefWhenFirstProtecting) {
  std::vector<PrefHashFilter::TrackedPreferenceMetadata>
      original_configuration = configuration_;
  for (std::vector<PrefHashFilter::TrackedPreferenceMetadata>::iterator it =
           configuration_.begin();
       it != configuration_.end();
       ++it) {
    it->enforcement_level = PrefHashFilter::NO_ENFORCEMENT;
  }
  ReloadConfiguration();

  InitializePrefs();

  ExpectValidationObserved(kTrackedAtomic);
  ExpectValidationObserved(kProtectedAtomic);

  LoadExistingPrefs();
  ExpectStringValueEquals(kUnprotectedPref, kFoobar);

  // Ensure everything is written out to disk.
  DestroyPrefStore();

  // Now introduce protection, including the never-before tracked "new_pref".
  configuration_ = original_configuration;
  PrefHashFilter::TrackedPreferenceMetadata new_protected = {
      kExtraReportingId, kUnprotectedPref, PrefHashFilter::ENFORCE_ON_LOAD,
      PrefHashFilter::TRACKING_STRATEGY_ATOMIC};
  configuration_.push_back(new_protected);
  ReloadConfiguration();

  // And try loading with the new configuration.
  LoadExistingPrefs();

  // Since there was a valid super MAC we were able to extend the existing trust
  // to the newly tracked & protected preference.
  ExpectStringValueEquals(kUnprotectedPref, kFoobar);
  VerifyResetRecorded(false);
}

TEST_F(ProfilePrefStoreManagerTest, UnprotectedToProtectedWithoutTrust) {
  InitializePrefs();

  ExpectValidationObserved(kTrackedAtomic);
  ExpectValidationObserved(kProtectedAtomic);

  // Now update the configuration to protect it.
  PrefHashFilter::TrackedPreferenceMetadata new_protected = {
      kExtraReportingId, kUnprotectedPref, PrefHashFilter::ENFORCE_ON_LOAD,
      PrefHashFilter::TRACKING_STRATEGY_ATOMIC};
  configuration_.push_back(new_protected);
  seed_ = "new-seed-to-break-trust";
  ReloadConfiguration();

  // And try loading with the new configuration.
  LoadExistingPrefs();

  // If preference tracking is supported, kUnprotectedPref will have been
  // discarded because new values are not accepted without a valid super MAC.
  EXPECT_NE(ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            pref_store_->GetValue(kUnprotectedPref, NULL));
  VerifyResetRecorded(
      ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking);
}

// This test verifies that preference values are correctly maintained when a
// preference's protection state changes from protected to unprotected.
TEST_F(ProfilePrefStoreManagerTest, ProtectedToUnprotected) {
  InitializePrefs();

  ExpectValidationObserved(kTrackedAtomic);
  ExpectValidationObserved(kProtectedAtomic);

  DestroyPrefStore();

  // Unconfigure protection for kProtectedAtomic
  for (std::vector<PrefHashFilter::TrackedPreferenceMetadata>::iterator it =
           configuration_.begin();
       it != configuration_.end();
       ++it) {
    if (it->name == kProtectedAtomic) {
      it->enforcement_level = PrefHashFilter::NO_ENFORCEMENT;
      break;
    }
  }

  seed_ = "new-seed-to-break-trust";
  ReloadConfiguration();
  LoadExistingPrefs();

  // Verify that the value was not reset.
  ExpectStringValueEquals(kProtectedAtomic, kHelloWorld);
  VerifyResetRecorded(false);

  // Accessing the value of the previously protected pref didn't trigger its
  // move to the unprotected preferences file, though the loading of the pref
  // store should still have caused the MAC store to be recalculated.
  LoadExistingPrefs();
  ExpectStringValueEquals(kProtectedAtomic, kHelloWorld);

  // Trigger the logic that migrates it back to the unprotected preferences
  // file.
  pref_store_->SetValue(kProtectedAtomic,
                        base::WrapUnique(new base::StringValue(kGoodbyeWorld)),
                        WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  LoadExistingPrefs();
  ExpectStringValueEquals(kProtectedAtomic, kGoodbyeWorld);
  VerifyResetRecorded(false);
}
