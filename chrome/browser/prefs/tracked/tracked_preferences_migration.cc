// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/tracked/tracked_preferences_migration.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/prefs/interceptable_pref_filter.h"

namespace {

class TrackedPreferencesMigrator
    : public base::RefCounted<TrackedPreferencesMigrator> {
 public:
  TrackedPreferencesMigrator(
      const std::set<std::string>& unprotected_pref_names,
      const std::set<std::string>& protected_pref_names,
      const base::Callback<void(const std::string& key)>&
          unprotected_store_cleaner,
      const base::Callback<void(const std::string& key)>&
          protected_store_cleaner,
      const base::Callback<void(const base::Closure&)>&
          register_on_successful_unprotected_store_write_callback,
      const base::Callback<void(const base::Closure&)>&
          register_on_successful_protected_store_write_callback,
      InterceptablePrefFilter* unprotected_pref_filter,
      InterceptablePrefFilter* protected_pref_filter);

 private:
  friend class base::RefCounted<TrackedPreferencesMigrator>;

  enum PrefFilterID {
    UNPROTECTED_PREF_FILTER,
    PROTECTED_PREF_FILTER
  };

  ~TrackedPreferencesMigrator();

  // Stores the data coming in from the filter identified by |id| into this
  // class and then calls MigrateIfReady();
  void InterceptFilterOnLoad(
      PrefFilterID id,
      const InterceptablePrefFilter::FinalizeFilterOnLoadCallback&
          finalize_filter_on_load,
      scoped_ptr<base::DictionaryValue> prefs);

  // Proceeds with migration if both |unprotected_prefs_| and |protected_prefs_|
  // have been set.
  void MigrateIfReady();

  const std::set<std::string> unprotected_pref_names_;
  const std::set<std::string> protected_pref_names_;

  const base::Callback<void(const std::string& key)> unprotected_store_cleaner_;
  const base::Callback<void(const std::string& key)> protected_store_cleaner_;
  const base::Callback<void(const base::Closure&)>
      register_on_successful_unprotected_store_write_callback_;
  const base::Callback<void(const base::Closure&)>
      register_on_successful_protected_store_write_callback_;

  InterceptablePrefFilter::FinalizeFilterOnLoadCallback
      finalize_unprotected_filter_on_load_;
  InterceptablePrefFilter::FinalizeFilterOnLoadCallback
      finalize_protected_filter_on_load_;

  scoped_ptr<base::DictionaryValue> unprotected_prefs_;
  scoped_ptr<base::DictionaryValue> protected_prefs_;

  DISALLOW_COPY_AND_ASSIGN(TrackedPreferencesMigrator);
};

// Invokes |store_cleaner| for every |keys_to_clean|.
void CleanupPrefStore(
    const base::Callback<void(const std::string& key)>& store_cleaner,
    const std::set<std::string>& keys_to_clean) {
  for (std::set<std::string>::const_iterator it = keys_to_clean.begin();
       it != keys_to_clean.end(); ++it) {
    store_cleaner.Run(*it);
  }
}

// If |wait_for_commit_to_destination_store|: schedules (via
// |register_on_successful_destination_store_write_callback|) a cleanup of the
// |keys_to_clean| from the source pref store (through |source_store_cleaner|)
// once the destination pref store they were migrated to was successfully
// written to disk. Otherwise, executes the cleanup right away.
void ScheduleSourcePrefStoreCleanup(
    const base::Callback<void(const base::Closure&)>&
        register_on_successful_destination_store_write_callback,
    const base::Callback<void(const std::string& key)>& source_store_cleaner,
    const std::set<std::string>& keys_to_clean,
    bool wait_for_commit_to_destination_store) {
  DCHECK(!keys_to_clean.empty());
  if (wait_for_commit_to_destination_store) {
    register_on_successful_destination_store_write_callback.Run(
        base::Bind(&CleanupPrefStore, source_store_cleaner, keys_to_clean));
  } else {
    CleanupPrefStore(source_store_cleaner, keys_to_clean);
  }
}

// Copies the value of each pref in |pref_names| which is set in |old_store|,
// but not in |new_store| into |new_store|. Sets |old_store_needs_cleanup| to
// true if any old duplicates remain in |old_store| and sets |new_store_altered|
// to true if any value was copied to |new_store|.
void MigratePrefsFromOldToNewStore(const std::set<std::string>& pref_names,
                                   const base::DictionaryValue* old_store,
                                   base::DictionaryValue* new_store,
                                   bool* old_store_needs_cleanup,
                                   bool* new_store_altered) {
  for (std::set<std::string>::const_iterator it = pref_names.begin();
       it != pref_names.end(); ++it) {
    const std::string& pref_name = *it;

    const base::Value* value_in_old_store = NULL;
    if (!old_store->Get(pref_name, &value_in_old_store))
      continue;

    // Whether this value ends up being copied below or was left behind by a
    // previous incomplete migration, it should be cleaned up.
    *old_store_needs_cleanup = true;

    if (new_store->Get(pref_name, NULL))
      continue;

    // Copy the value from |old_store| to |new_store| rather than moving it to
    // avoid data loss should |old_store| be flushed to disk without |new_store|
    // having equivalently been successfully flushed to disk (e.g., on crash or
    // in cases where |new_store| is read-only following a read error on
    // startup).
    new_store->Set(pref_name, value_in_old_store->DeepCopy());
    *new_store_altered = true;
  }
}

TrackedPreferencesMigrator::TrackedPreferencesMigrator(
    const std::set<std::string>& unprotected_pref_names,
    const std::set<std::string>& protected_pref_names,
    const base::Callback<void(const std::string& key)>&
        unprotected_store_cleaner,
    const base::Callback<void(const std::string& key)>& protected_store_cleaner,
    const base::Callback<void(const base::Closure&)>&
        register_on_successful_unprotected_store_write_callback,
    const base::Callback<void(const base::Closure&)>&
        register_on_successful_protected_store_write_callback,
    InterceptablePrefFilter* unprotected_pref_filter,
    InterceptablePrefFilter* protected_pref_filter)
    : unprotected_pref_names_(unprotected_pref_names),
      protected_pref_names_(protected_pref_names),
      unprotected_store_cleaner_(unprotected_store_cleaner),
      protected_store_cleaner_(protected_store_cleaner),
      register_on_successful_unprotected_store_write_callback_(
          register_on_successful_unprotected_store_write_callback),
      register_on_successful_protected_store_write_callback_(
          register_on_successful_protected_store_write_callback) {
  // The callbacks bound below will own this TrackedPreferencesMigrator by
  // reference.
  unprotected_pref_filter->InterceptNextFilterOnLoad(
      base::Bind(&TrackedPreferencesMigrator::InterceptFilterOnLoad,
                 this,
                 UNPROTECTED_PREF_FILTER));
  protected_pref_filter->InterceptNextFilterOnLoad(
      base::Bind(&TrackedPreferencesMigrator::InterceptFilterOnLoad,
                 this,
                 PROTECTED_PREF_FILTER));
}

TrackedPreferencesMigrator::~TrackedPreferencesMigrator() {}

void TrackedPreferencesMigrator::InterceptFilterOnLoad(
    PrefFilterID id,
    const InterceptablePrefFilter::FinalizeFilterOnLoadCallback&
        finalize_filter_on_load,
    scoped_ptr<base::DictionaryValue> prefs) {
  switch (id) {
    case UNPROTECTED_PREF_FILTER:
      finalize_unprotected_filter_on_load_ = finalize_filter_on_load;
      unprotected_prefs_ = prefs.Pass();
      break;
    case PROTECTED_PREF_FILTER:
      finalize_protected_filter_on_load_ = finalize_filter_on_load;
      protected_prefs_ = prefs.Pass();
      break;
  }

  MigrateIfReady();
}

void TrackedPreferencesMigrator::MigrateIfReady() {
  // Wait for both stores to have been read before proceeding.
  if (!protected_prefs_ || !unprotected_prefs_)
    return;

  bool protected_prefs_need_cleanup = false;
  bool unprotected_prefs_altered = false;
  MigratePrefsFromOldToNewStore(unprotected_pref_names_,
                                protected_prefs_.get(),
                                unprotected_prefs_.get(),
                                &protected_prefs_need_cleanup,
                                &unprotected_prefs_altered);
  bool unprotected_prefs_need_cleanup = false;
  bool protected_prefs_altered = false;
  MigratePrefsFromOldToNewStore(protected_pref_names_,
                                unprotected_prefs_.get(),
                                protected_prefs_.get(),
                                &unprotected_prefs_need_cleanup,
                                &protected_prefs_altered);

  // Hand the processed prefs back to their respective filters.
  finalize_unprotected_filter_on_load_.Run(unprotected_prefs_.Pass(),
                                           unprotected_prefs_altered);
  finalize_protected_filter_on_load_.Run(protected_prefs_.Pass(),
                                         protected_prefs_altered);

  if (unprotected_prefs_need_cleanup) {
    // Schedule a cleanup of the |protected_pref_names_| from the unprotected
    // prefs once the protected prefs were successfully written to disk (or
    // do it immediately if |!protected_prefs_altered|).
    ScheduleSourcePrefStoreCleanup(
        register_on_successful_protected_store_write_callback_,
        unprotected_store_cleaner_,
        protected_pref_names_,
        protected_prefs_altered);
  }

  if (protected_prefs_need_cleanup) {
    // Schedule a cleanup of the |unprotected_pref_names_| from the protected
    // prefs once the unprotected prefs were successfully written to disk (or
    // do it immediately if |!unprotected_prefs_altered|).
    ScheduleSourcePrefStoreCleanup(
        register_on_successful_unprotected_store_write_callback_,
        protected_store_cleaner_,
        unprotected_pref_names_,
        unprotected_prefs_altered);
  }
}

}  // namespace

void SetupTrackedPreferencesMigration(
    const std::set<std::string>& unprotected_pref_names,
    const std::set<std::string>& protected_pref_names,
    const base::Callback<void(const std::string& key)>&
        unprotected_store_cleaner,
    const base::Callback<void(const std::string& key)>& protected_store_cleaner,
    const base::Callback<void(const base::Closure&)>&
        register_on_successful_unprotected_store_write_callback,
    const base::Callback<void(const base::Closure&)>&
        register_on_successful_protected_store_write_callback,
    InterceptablePrefFilter* unprotected_pref_filter,
    InterceptablePrefFilter* protected_pref_filter) {
  scoped_refptr<TrackedPreferencesMigrator> prefs_migrator(
      new TrackedPreferencesMigrator(
          unprotected_pref_names,
          protected_pref_names,
          unprotected_store_cleaner,
          protected_store_cleaner,
          register_on_successful_unprotected_store_write_callback,
          register_on_successful_protected_store_write_callback,
          unprotected_pref_filter,
          protected_pref_filter));
}
