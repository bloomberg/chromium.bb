// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_WEBDATA_AUTOFILL_PROFILE_SYNCABLE_SERVICE_H_
#define CHROME_BROWSER_WEBDATA_AUTOFILL_PROFILE_SYNCABLE_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "base/supports_user_data.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "components/autofill/browser/autofill_type.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/syncable_service.h"
#include "sync/protocol/autofill_specifics.pb.h"

class AutofillProfile;
class AutofillTable;
class AutofillWebDataService;
class FormGroup;
class ProfileSyncServiceAutofillTest;
class WebDataServiceBase;

extern const char kAutofillProfileTag[];

// The sync implementation for AutofillProfiles.
// MergeDataAndStartSyncing() called first, it does cloud->local and
// local->cloud syncs. Then for each cloud change we receive
// ProcessSyncChanges() and for each local change Observe() is called.
class AutofillProfileSyncableService
    : public base::SupportsUserData::Data,
      public syncer::SyncableService,
      public content::NotificationObserver,
      public base::NonThreadSafe {
 public:
  virtual ~AutofillProfileSyncableService();

  // Creates a new AutofillProfileSyncableService and hangs it off of
  // |web_data_service|, which takes ownership.
  static void CreateForWebDataService(AutofillWebDataService* web_data_service);
  // Retrieves the AutofillProfileSyncableService stored on |web_data_service|.
  static AutofillProfileSyncableService* FromWebDataService(
      AutofillWebDataService* web_data_service);

  static syncer::ModelType model_type() { return syncer::AUTOFILL_PROFILE; }

  // syncer::SyncableService implementation.
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(
      syncer::ModelType type) const OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  explicit AutofillProfileSyncableService(
      AutofillWebDataService* web_data_service);

  // A convenience wrapper of a bunch of state we pass around while
  // associating models, and send to the WebDatabase for persistence.
  // We do this so we hold the write lock for only a small period.
  // When storing the web db we are out of the write lock.
  struct DataBundle;

  // Helper to query WebDatabase for the current autofill state.
  // Made virtual for ease of mocking in the unit-test.
  // Caller owns returned |profiles|.
  virtual bool LoadAutofillData(std::vector<AutofillProfile*>* profiles);

  // Helper to persist any changes that occured during model association to
  // the WebDatabase.
  // Made virtual for ease of mocking in the unit-test.
  virtual bool SaveChangesToWebData(const DataBundle& bundle);

 private:
  friend class ProfileSyncServiceAutofillTest;
  friend class MockAutofillProfileSyncableService;
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest,
                           MergeDataAndStartSyncing);
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest, GetAllSyncData);
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest,
                           ProcessSyncChanges);
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest,
                           ActOnChange);
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest,
                           UpdateField);
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest,
                           UpdateMultivaluedField);
  FRIEND_TEST_ALL_PREFIXES(AutofillProfileSyncableServiceTest,
                           MergeProfile);

  // The map of the guid to profiles owned by the |profiles_| vector.
  typedef std::map<std::string, AutofillProfile*> GUIDToProfileMap;

  // Helper function that overwrites |profile| with data from proto-buffer
  // |specifics|.
  static bool OverwriteProfileWithServerData(
      const sync_pb::AutofillProfileSpecifics& specifics,
      AutofillProfile* profile);

  // Writes |profile| data into supplied |profile_specifics|.
  static void WriteAutofillProfile(const AutofillProfile& profile,
                                   sync_pb::EntitySpecifics* profile_specifics);

  // Creates |profile_map| from the supplied |profiles| vector. Necessary for
  // fast processing of the changes.
  void CreateGUIDToProfileMap(const std::vector<AutofillProfile*>& profiles,
                              GUIDToProfileMap* profile_map);

  // Creates or updates a profile based on |data|. Looks at the guid of the data
  // and if a profile with such guid is present in |profile_map| updates it. If
  // not, searches through it for similar profiles. If similar profile is
  // found substitutes it for the new one, otherwise adds a new profile. Returns
  // iterator pointing to added/updated profile.
  GUIDToProfileMap::iterator CreateOrUpdateProfile(
      const syncer::SyncData& data,
      GUIDToProfileMap* profile_map,
      DataBundle* bundle);

  // Syncs |change| to the cloud.
  void ActOnChange(const AutofillProfileChange& change);

  // Creates syncer::SyncData based on supplied |profile|.
  static syncer::SyncData CreateData(const AutofillProfile& profile);

  AutofillTable* GetAutofillTable() const;

  // Helper to compare the local value and cloud value of a field, copy into
  // the local value if they differ, and return whether the change happened.
  static bool UpdateField(AutofillFieldType field_type,
                          const std::string& new_value,
                          AutofillProfile* autofill_profile);
  // The same as |UpdateField|, but for multi-valued fields.
  static bool UpdateMultivaluedField(
      AutofillFieldType field_type,
      const ::google::protobuf::RepeatedPtrField<std::string>& new_value,
      AutofillProfile* autofill_profile);

  // Calls merge_into->OverwriteWithOrAddTo() and then checks if the
  // |merge_into| has extra data. Returns |true| if |merge_into| posseses some
  // multi-valued field values that are not in |merge_from|, false otherwise.
  static bool MergeProfile(const AutofillProfile& merge_from,
                           AutofillProfile* merge_into);

  // For unit-tests.
  AutofillProfileSyncableService();
  void set_sync_processor(syncer::SyncChangeProcessor* sync_processor) {
    sync_processor_.reset(sync_processor);
  }

  AutofillWebDataService* web_data_service_;  // WEAK
  content::NotificationRegistrar notification_registrar_;

  // Cached Autofill profiles. *Warning* deleted profiles are still in the
  // vector - use the |profiles_map_| to iterate through actual profiles.
  ScopedVector<AutofillProfile> profiles_;
  GUIDToProfileMap profiles_map_;

  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;

  scoped_ptr<syncer::SyncErrorFactory> sync_error_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillProfileSyncableService);
};

// This object is used in unit-tests as well, so it defined here.
struct AutofillProfileSyncableService::DataBundle {
  DataBundle();
  ~DataBundle();

  std::vector<std::string> profiles_to_delete;
  std::vector<AutofillProfile*> profiles_to_update;
  std::vector<AutofillProfile*> profiles_to_add;

  // When we go through sync we find profiles that are similar but unmatched.
  // Merge such profiles.
  GUIDToProfileMap candidates_to_merge;
  // Profiles that have multi-valued fields that are not in sync.
  std::vector<AutofillProfile*> profiles_to_sync_back;
};

#endif  // CHROME_BROWSER_WEBDATA_AUTOFILL_PROFILE_SYNCABLE_SERVICE_H_
