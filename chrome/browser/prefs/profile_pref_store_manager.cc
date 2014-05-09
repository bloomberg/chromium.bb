// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/profile_pref_store_manager.h"

#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/persistent_pref_store.h"
#include "base/prefs/pref_registry_simple.h"
#include "chrome/browser/prefs/pref_hash_store_impl.h"
#include "chrome/browser/prefs/tracked/pref_service_hash_store_contents.h"
#include "chrome/browser/prefs/tracked/segregated_pref_store.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"

namespace {

// An adaptor that allows a PrefHashStoreImpl to access a preference store
// directly as a dictionary. Uses an equivalent layout to
// PrefStoreHashStoreContents.
class DictionaryHashStoreContents : public HashStoreContents {
 public:
  // Instantiates a HashStoreContents that is a copy of |to_copy|. The copy is
  // mutable but does not affect the original, nor is it persisted to disk in
  // any other way.
  explicit DictionaryHashStoreContents(const HashStoreContents& to_copy)
      : hash_store_id_(to_copy.hash_store_id()),
        super_mac_(to_copy.GetSuperMac()) {
    if (to_copy.IsInitialized())
      dictionary_.reset(to_copy.GetContents()->DeepCopy());
    int version = 0;
    if (to_copy.GetVersion(&version))
      version_.reset(new int(version));
  }

  // HashStoreContents implementation
  virtual std::string hash_store_id() const OVERRIDE { return hash_store_id_; }

  virtual void Reset() OVERRIDE {
    dictionary_.reset();
    super_mac_.clear();
    version_.reset();
  }

  virtual bool IsInitialized() const OVERRIDE {
    return dictionary_;
  }

  virtual const base::DictionaryValue* GetContents() const OVERRIDE{
    return dictionary_.get();
  }

  virtual scoped_ptr<MutableDictionary> GetMutableContents() OVERRIDE {
    return scoped_ptr<MutableDictionary>(
        new SimpleMutableDictionary(this));
  }

  virtual std::string GetSuperMac() const OVERRIDE { return super_mac_; }

  virtual void SetSuperMac(const std::string& super_mac) OVERRIDE {
    super_mac_ = super_mac;
  }

  virtual bool GetVersion(int* version) const OVERRIDE {
    if (!version_)
      return false;
    *version = *version_;
    return true;
  }

  virtual void SetVersion(int version) OVERRIDE {
    version_.reset(new int(version));
  }

  virtual void CommitPendingWrite() OVERRIDE {}

 private:
  class SimpleMutableDictionary
      : public HashStoreContents::MutableDictionary {
   public:
    explicit SimpleMutableDictionary(DictionaryHashStoreContents* outer)
        : outer_(outer) {}

    virtual ~SimpleMutableDictionary() {}

    // MutableDictionary implementation
    virtual base::DictionaryValue* operator->() OVERRIDE {
      if (!outer_->dictionary_)
        outer_->dictionary_.reset(new base::DictionaryValue);
      return outer_->dictionary_.get();
    }

   private:
    DictionaryHashStoreContents* outer_;

    DISALLOW_COPY_AND_ASSIGN(SimpleMutableDictionary);
  };

  const std::string hash_store_id_;
  std::string super_mac_;
  scoped_ptr<int> version_;
  scoped_ptr<base::DictionaryValue> dictionary_;

  DISALLOW_COPY_AND_ASSIGN(DictionaryHashStoreContents);
};

// An in-memory PrefStore backed by an immutable DictionaryValue.
class DictionaryPrefStore : public PrefStore {
 public:
  explicit DictionaryPrefStore(const base::DictionaryValue* dictionary)
      : dictionary_(dictionary) {}

  virtual bool GetValue(const std::string& key,
                        const base::Value** result) const OVERRIDE {
    const base::Value* tmp = NULL;
    if (!dictionary_->Get(key, &tmp))
      return false;

    if (result)
      *result = tmp;
    return true;
  }

 private:
  virtual ~DictionaryPrefStore() {}

  const base::DictionaryValue* dictionary_;

  DISALLOW_COPY_AND_ASSIGN(DictionaryPrefStore);
};

// Waits for a PrefStore to be initialized and then initializes the
// corresponding PrefHashStore.
// The observer deletes itself when its work is completed.
class InitializeHashStoreObserver : public PrefStore::Observer {
 public:
  // Creates an observer that will initialize |pref_hash_store| with the
  // contents of |pref_store| when the latter is fully loaded.
  InitializeHashStoreObserver(
      const std::vector<PrefHashFilter::TrackedPreferenceMetadata>&
          tracking_configuration,
      size_t reporting_ids_count,
      const scoped_refptr<PrefStore>& pref_store,
      scoped_ptr<PrefHashStoreImpl> pref_hash_store_impl)
      : tracking_configuration_(tracking_configuration),
        reporting_ids_count_(reporting_ids_count),
        pref_store_(pref_store),
        pref_hash_store_impl_(pref_hash_store_impl.Pass()) {}

  virtual ~InitializeHashStoreObserver();

  // PrefStore::Observer implementation.
  virtual void OnPrefValueChanged(const std::string& key) OVERRIDE;
  virtual void OnInitializationCompleted(bool succeeded) OVERRIDE;

 private:
  const std::vector<PrefHashFilter::TrackedPreferenceMetadata>
      tracking_configuration_;
  const size_t reporting_ids_count_;
  scoped_refptr<PrefStore> pref_store_;
  scoped_ptr<PrefHashStoreImpl> pref_hash_store_impl_;

  DISALLOW_COPY_AND_ASSIGN(InitializeHashStoreObserver);
};

InitializeHashStoreObserver::~InitializeHashStoreObserver() {}

void InitializeHashStoreObserver::OnPrefValueChanged(const std::string& key) {}

void InitializeHashStoreObserver::OnInitializationCompleted(bool succeeded) {
  // If we successfully loaded the preferences _and_ the PrefHashStoreImpl
  // hasn't been initialized by someone else in the meantime, initialize it now.
  const PrefHashStoreImpl::StoreVersion pre_update_version =
      pref_hash_store_impl_->GetCurrentVersion();
  if (succeeded && pre_update_version < PrefHashStoreImpl::VERSION_LATEST) {
    PrefHashFilter(pref_hash_store_impl_.PassAs<PrefHashStore>(),
                   tracking_configuration_,
                   reporting_ids_count_).Initialize(*pref_store_);
    UMA_HISTOGRAM_ENUMERATION(
        "Settings.TrackedPreferencesAlternateStoreVersionUpdatedFrom",
        pre_update_version,
        PrefHashStoreImpl::VERSION_LATEST + 1);
  }
  pref_store_->RemoveObserver(this);
  delete this;
}

}  // namespace

// TODO(erikwright): Enable this on Chrome OS and Android once MACs are moved
// out of Local State. This will resolve a race condition on Android and a
// privacy issue on ChromeOS. http://crbug.com/349158
const bool ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking =
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    false;
#else
    true;
#endif

ProfilePrefStoreManager::ProfilePrefStoreManager(
    const base::FilePath& profile_path,
    const std::vector<PrefHashFilter::TrackedPreferenceMetadata>&
        tracking_configuration,
    size_t reporting_ids_count,
    const std::string& seed,
    const std::string& device_id,
    PrefService* local_state)
    : profile_path_(profile_path),
      tracking_configuration_(tracking_configuration),
      reporting_ids_count_(reporting_ids_count),
      seed_(seed),
      device_id_(device_id),
      local_state_(local_state) {}

ProfilePrefStoreManager::~ProfilePrefStoreManager() {}

// static
void ProfilePrefStoreManager::RegisterPrefs(PrefRegistrySimple* registry) {
  PrefServiceHashStoreContents::RegisterPrefs(registry);
}

// static
void ProfilePrefStoreManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  PrefHashFilter::RegisterProfilePrefs(registry);
}

// static
base::FilePath ProfilePrefStoreManager::GetPrefFilePathFromProfilePath(
    const base::FilePath& profile_path) {
  return profile_path.Append(chrome::kPreferencesFilename);
}

// static
void ProfilePrefStoreManager::ResetAllPrefHashStores(PrefService* local_state) {
  PrefServiceHashStoreContents::ResetAllPrefHashStores(local_state);
}

//  static
base::Time ProfilePrefStoreManager::GetResetTime(PrefService* pref_service) {
  // It's a bit of a coincidence that this (and ClearResetTime) work(s). The
  // PrefHashFilter attached to the protected pref store will store the reset
  // time directly in the protected pref store without going through the
  // SegregatedPrefStore.

  // PrefHashFilter::GetResetTime will read the value through the pref service,
  // and thus through the SegregatedPrefStore. Even though it's not listed as
  // "protected" it will be read from the protected store preferentially to the
  // (NULL) value in the unprotected pref store.
  return PrefHashFilter::GetResetTime(pref_service);
}

// static
void ProfilePrefStoreManager::ClearResetTime(PrefService* pref_service) {
  // PrefHashFilter::ClearResetTime will clear the value through the pref
  // service, and thus through the SegregatedPrefStore. Since it's not listed as
  // "protected" it will be migrated from the protected store to the unprotected
  // pref store before being deleted from the latter.
  PrefHashFilter::ClearResetTime(pref_service);
}

void ProfilePrefStoreManager::ResetPrefHashStore() {
  if (kPlatformSupportsPreferenceTracking)
    GetPrefHashStoreImpl()->Reset();
}

PersistentPrefStore* ProfilePrefStoreManager::CreateProfilePrefStore(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner) {
  scoped_ptr<PrefFilter> pref_filter;
  if (!kPlatformSupportsPreferenceTracking) {
    return new JsonPrefStore(GetPrefFilePathFromProfilePath(profile_path_),
                             io_task_runner,
                             scoped_ptr<PrefFilter>());
  }

  std::vector<PrefHashFilter::TrackedPreferenceMetadata>
      unprotected_configuration;
  std::vector<PrefHashFilter::TrackedPreferenceMetadata>
      protected_configuration;
  std::set<std::string> protected_pref_names;
  for (std::vector<PrefHashFilter::TrackedPreferenceMetadata>::const_iterator
           it = tracking_configuration_.begin();
       it != tracking_configuration_.end();
       ++it) {
    if (it->enforcement_level > PrefHashFilter::NO_ENFORCEMENT) {
      protected_configuration.push_back(*it);
      protected_pref_names.insert(it->name);
    } else {
      unprotected_configuration.push_back(*it);
    }
  }

  scoped_ptr<PrefFilter> unprotected_pref_hash_filter(
      new PrefHashFilter(GetPrefHashStoreImpl().PassAs<PrefHashStore>(),
                         unprotected_configuration,
                         reporting_ids_count_));
  scoped_ptr<PrefFilter> protected_pref_hash_filter(
      new PrefHashFilter(GetPrefHashStoreImpl().PassAs<PrefHashStore>(),
                         protected_configuration,
                         reporting_ids_count_));

  scoped_refptr<PersistentPrefStore> unprotected_pref_store(
      new JsonPrefStore(GetPrefFilePathFromProfilePath(profile_path_),
                        io_task_runner,
                        unprotected_pref_hash_filter.Pass()));
  scoped_refptr<PersistentPrefStore> protected_pref_store(new JsonPrefStore(
      profile_path_.Append(chrome::kProtectedPreferencesFilename),
      io_task_runner,
      protected_pref_hash_filter.Pass()));

  // The on_initialized callback is used to migrate newly protected values from
  // the main Preferences store to the Protected Preferences store. It is also
  // responsible for the initial migration to a two-store model.
  return new SegregatedPrefStore(
      unprotected_pref_store,
      protected_pref_store,
      protected_pref_names,
      base::Bind(&PrefHashFilter::MigrateValues,
                 base::Owned(new PrefHashFilter(
                     CopyPrefHashStore(),
                     protected_configuration,
                     reporting_ids_count_)),
                 unprotected_pref_store,
                 protected_pref_store));
}

void ProfilePrefStoreManager::UpdateProfileHashStoreIfRequired(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner) {
  if (!kPlatformSupportsPreferenceTracking)
    return;
  scoped_ptr<PrefHashStoreImpl> pref_hash_store_impl(GetPrefHashStoreImpl());
  const PrefHashStoreImpl::StoreVersion current_version =
      pref_hash_store_impl->GetCurrentVersion();
  UMA_HISTOGRAM_ENUMERATION("Settings.TrackedPreferencesAlternateStoreVersion",
                            current_version,
                            PrefHashStoreImpl::VERSION_LATEST + 1);

  // Update the pref hash store if it's not at the latest version.
  if (current_version != PrefHashStoreImpl::VERSION_LATEST) {
    scoped_refptr<JsonPrefStore> pref_store =
        new JsonPrefStore(GetPrefFilePathFromProfilePath(profile_path_),
                          io_task_runner,
                          scoped_ptr<PrefFilter>());
    pref_store->AddObserver(
        new InitializeHashStoreObserver(tracking_configuration_,
                                        reporting_ids_count_,
                                        pref_store,
                                        pref_hash_store_impl.Pass()));
    pref_store->ReadPrefsAsync(NULL);
  }
}

bool ProfilePrefStoreManager::InitializePrefsFromMasterPrefs(
    const base::DictionaryValue& master_prefs) {
  // Create the profile directory if it doesn't exist yet (very possible on
  // first run).
  if (!base::CreateDirectory(profile_path_))
    return false;

  // This will write out to a single combined file which will be immediately
  // migrated to two files on load.
  JSONFileValueSerializer serializer(
      GetPrefFilePathFromProfilePath(profile_path_));

  // Call Serialize (which does IO) on the main thread, which would _normally_
  // be verboten. In this case however, we require this IO to synchronously
  // complete before Chrome can start (as master preferences seed the Local
  // State and Preferences files). This won't trip ThreadIORestrictions as they
  // won't have kicked in yet on the main thread.
  bool success = serializer.Serialize(master_prefs);

  if (success && kPlatformSupportsPreferenceTracking) {
    scoped_refptr<const PrefStore> pref_store(
        new DictionaryPrefStore(&master_prefs));
    PrefHashFilter(GetPrefHashStoreImpl().PassAs<PrefHashStore>(),
                   tracking_configuration_,
                   reporting_ids_count_).Initialize(*pref_store);
  }

  UMA_HISTOGRAM_BOOLEAN("Settings.InitializedFromMasterPrefs", success);
  return success;
}

PersistentPrefStore*
ProfilePrefStoreManager::CreateDeprecatedCombinedProfilePrefStore(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner) {
  scoped_ptr<PrefFilter> pref_filter;
  if (kPlatformSupportsPreferenceTracking) {
    pref_filter.reset(
        new PrefHashFilter(GetPrefHashStoreImpl().PassAs<PrefHashStore>(),
                           tracking_configuration_,
                           reporting_ids_count_));
  }
  return new JsonPrefStore(GetPrefFilePathFromProfilePath(profile_path_),
                           io_task_runner,
                           pref_filter.Pass());
}

scoped_ptr<PrefHashStoreImpl> ProfilePrefStoreManager::GetPrefHashStoreImpl() {
  DCHECK(kPlatformSupportsPreferenceTracking);

  return make_scoped_ptr(new PrefHashStoreImpl(
      seed_,
      device_id_,
      scoped_ptr<HashStoreContents>(new PrefServiceHashStoreContents(
          profile_path_.AsUTF8Unsafe(), local_state_))));
}

scoped_ptr<PrefHashStore> ProfilePrefStoreManager::CopyPrefHashStore() {
  DCHECK(kPlatformSupportsPreferenceTracking);

  PrefServiceHashStoreContents real_contents(profile_path_.AsUTF8Unsafe(),
                                             local_state_);
  return scoped_ptr<PrefHashStore>(new PrefHashStoreImpl(
      seed_,
      device_id_,
      scoped_ptr<HashStoreContents>(
          new DictionaryHashStoreContents(real_contents))));
}
