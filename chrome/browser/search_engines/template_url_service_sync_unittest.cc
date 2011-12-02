// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/protocol/search_engine_specifics.pb.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

namespace {

// Extract the GUID from a search engine SyncData.
std::string GetGUID(const SyncData& sync_data) {
  return sync_data.GetSpecifics().GetExtension(
      sync_pb::search_engine).sync_guid();
}

// Extract the URL from a search engine SyncData.
std::string GetURL(const SyncData& sync_data) {
  return sync_data.GetSpecifics().GetExtension(
      sync_pb::search_engine).url();
}

// Extract the keyword from a search engine SyncData.
std::string GetKeyword(const SyncData& sync_data) {
  return sync_data.GetSpecifics().GetExtension(
      sync_pb::search_engine).keyword();
}

// TODO(stevet): Share these with template_url_service_unittest.
// Set the managed preferences for the default search provider and trigger
// notification.
void SetManagedDefaultSearchPreferences(TemplateURLService* turl_service,
                                        TestingProfile* profile,
                                        bool enabled,
                                        const char* name,
                                        const char* search_url,
                                        const char* suggest_url,
                                        const char* icon_url,
                                        const char* encodings,
                                        const char* keyword) {
  TestingPrefService* pref_service = profile->GetTestingPrefService();
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderEnabled,
                               Value::CreateBooleanValue(enabled));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderName,
                               Value::CreateStringValue(name));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderSearchURL,
                               Value::CreateStringValue(search_url));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderSuggestURL,
                               Value::CreateStringValue(suggest_url));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderIconURL,
                               Value::CreateStringValue(icon_url));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderEncodings,
                               Value::CreateStringValue(encodings));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderKeyword,
                               Value::CreateStringValue(keyword));
}

// Remove all the managed preferences for the default search provider and
// trigger notification.
void RemoveManagedDefaultSearchPreferences(TemplateURLService* turl_service,
                                           TestingProfile* profile) {
  TestingPrefService* pref_service = profile->GetTestingPrefService();
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderSearchURL);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderEnabled);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderName);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderSuggestURL);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderIconURL);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderEncodings);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderKeyword);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderID);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderPrepopulateID);
}

// Dummy SyncChangeProcessor used to help review what SyncChanges are pushed
// back up to Sync.
class TestChangeProcessor : public SyncChangeProcessor {
 public:
  TestChangeProcessor() : erroneous_(false) {
  }
  virtual ~TestChangeProcessor() { }

  // Store a copy of all the changes passed in so we can examine them later.
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) {
    if (erroneous_)
      return SyncError(FROM_HERE, "Some error.", syncable::SEARCH_ENGINES);

    change_map_.erase(change_map_.begin(), change_map_.end());
    for (SyncChangeList::const_iterator iter = change_list.begin();
        iter != change_list.end(); ++iter) {
      change_map_[GetGUID(iter->sync_data())] = *iter;
    }

    return SyncError();
  }

  bool ContainsGUID(const std::string& guid) {
    return change_map_.find(guid) != change_map_.end();
  }

  SyncChange GetChangeByGUID(const std::string& guid) {
    DCHECK(ContainsGUID(guid));
    return change_map_[guid];
  }

  int change_list_size() { return change_map_.size(); }

  void set_erroneous(bool erroneous) { erroneous_ = erroneous; }

 private:
  // Track the changes received in ProcessSyncChanges.
  std::map<std::string, SyncChange> change_map_;
  bool erroneous_;

  DISALLOW_COPY_AND_ASSIGN(TestChangeProcessor);
};

class TemplateURLServiceSyncTest : public testing::Test {
 public:
  typedef TemplateURLService::SyncDataMap SyncDataMap;

  TemplateURLServiceSyncTest() {}

  virtual void SetUp() {
    profile_a_.reset(new TestingProfile);
    TemplateURLServiceFactory::GetInstance()->RegisterUserPrefsOnProfile(
        profile_a_.get());
    model_a_.reset(new TemplateURLService(profile_a_.get()));
    model_a_->Load();
    profile_b_.reset(new TestingProfile);
    TemplateURLServiceFactory::GetInstance()->RegisterUserPrefsOnProfile(
        profile_b_.get());
    model_b_.reset(new TemplateURLService(profile_b_.get()));
    model_b_->Load();
  }

  virtual void TearDown() { }

  TemplateURLService* model() { return model_a_.get(); }
  // For readability, we redefine an accessor for Model A for use in tests that
  // involve syncing two models.
  TemplateURLService* model_a() { return model_a_.get(); }
  TemplateURLService* model_b() { return model_b_.get(); }
  TestChangeProcessor* processor() { return &processor_; }

  // Create a TemplateURL with some test values. The caller owns the returned
  // TemplateURL*.
  TemplateURL* CreateTestTemplateURL() const {
    TemplateURL* turl = new TemplateURL();
    turl->SetURL("http://www.unittest.com/", 0, 0);
    turl->set_keyword(ASCIIToUTF16("unittest"));
    turl->set_short_name(ASCIIToUTF16("unittest"));
    turl->set_safe_for_autoreplace(true);
    GURL favicon_url("http://favicon.url");
    turl->SetFaviconURL(favicon_url);
    turl->set_date_created(Time::FromTimeT(100));
    turl->set_last_modified(Time::FromTimeT(100));
    turl->SetPrepopulateId(999999);
    return turl;
  }

  // Convenience helpers for creating TemplateURLs with specific fields.
  TemplateURL* CreateTestTemplateURL(std::string keyword,
                                     std::string url) const {
    TemplateURL* turl = CreateTestTemplateURL();
    turl->set_keyword(UTF8ToUTF16(keyword));
    turl->SetURL(url, 0, 0);
    return turl;
  }

  TemplateURL* CreateTestTemplateURL(std::string keyword,
                                     std::string url,
                                     std::string guid) const {
    TemplateURL* turl = CreateTestTemplateURL(keyword, url);
    if (!guid.empty())
      turl->set_sync_guid(guid);
    return turl;
  }

  TemplateURL* CreateTestTemplateURL(std::string keyword,
                                     std::string url,
                                     std::string guid,
                                     time_t last_mod) const {
    TemplateURL* turl = CreateTestTemplateURL(keyword, url, guid);
    turl->set_last_modified(Time::FromTimeT(last_mod));
    return turl;
  }

  // Verifies the two TemplateURLs are equal.
  // TODO(stevet): Share this with TemplateURLServiceTest.
  void AssertEquals(const TemplateURL& expected,
                    const TemplateURL& actual) const {
    ASSERT_TRUE(TemplateURLRef::SameUrlRefs(expected.url(), actual.url()));
    ASSERT_TRUE(TemplateURLRef::SameUrlRefs(expected.suggestions_url(),
                                            actual.suggestions_url()));
    ASSERT_EQ(expected.keyword(), actual.keyword());
    ASSERT_EQ(expected.short_name(), actual.short_name());
    ASSERT_EQ(JoinString(expected.input_encodings(), ';'),
              JoinString(actual.input_encodings(), ';'));
    ASSERT_TRUE(expected.GetFaviconURL() == actual.GetFaviconURL());
    ASSERT_EQ(expected.safe_for_autoreplace(), actual.safe_for_autoreplace());
    ASSERT_EQ(expected.show_in_default_list(), actual.show_in_default_list());
    ASSERT_TRUE(expected.date_created() == actual.date_created());
    ASSERT_TRUE(expected.last_modified() == actual.last_modified());
  }

  // Expect that two SyncDataLists have equal contents, in terms of the
  // sync_guid, keyword, and url fields.
  void AssertEquals(const SyncDataList& data1,
                    const SyncDataList& data2) const {
    SyncDataMap map1 = TemplateURLService::CreateGUIDToSyncDataMap(data1);
    SyncDataMap map2 = TemplateURLService::CreateGUIDToSyncDataMap(data2);

    for (SyncDataMap::const_iterator iter1 = map1.begin();
        iter1 != map1.end(); iter1++) {
      SyncDataMap::iterator iter2 = map2.find(iter1->first);
      if (iter2 != map2.end()) {
        ASSERT_EQ(GetKeyword(iter1->second), GetKeyword(iter2->second));
        ASSERT_EQ(GetURL(iter1->second), GetURL(iter2->second));
        map2.erase(iter2);
      }
    }
    EXPECT_EQ(0U, map2.size());
  }

  // Convenience helper for creating SyncChanges. Takes ownership of |turl|.
  SyncChange CreateTestSyncChange(SyncChange::SyncChangeType type,
                                  TemplateURL* turl) const {
    // We take control of the TemplateURL so make sure it's cleaned up after
    // we create data out of it.
    scoped_ptr<TemplateURL> scoped_turl(turl);
    return SyncChange(
        type, TemplateURLService::CreateSyncDataFromTemplateURL(*scoped_turl));
  }

  // Helper that creates some initial sync data. We cheat a little by specifying
  // GUIDs for easy identification later. We also make the last_modified times
  // slightly older than CreateTestTemplateURL's default, to test conflict
  // resolution.
  SyncDataList CreateInitialSyncData() const {
    SyncDataList list;

    scoped_ptr<TemplateURL> turl(
        CreateTestTemplateURL("key1", "http://key1.com", "key1", 90));
    list.push_back(TemplateURLService::CreateSyncDataFromTemplateURL(*turl));
    turl.reset(CreateTestTemplateURL("key2", "http://key2.com", "key2", 90));
    list.push_back(TemplateURLService::CreateSyncDataFromTemplateURL(*turl));
    turl.reset(CreateTestTemplateURL("key3", "http://key3.com", "key3", 90));
    list.push_back(TemplateURLService::CreateSyncDataFromTemplateURL(*turl));

    return list;
  }

 protected:
  // We keep two TemplateURLServices to test syncing between them.
  scoped_ptr<TestingProfile> profile_a_;
  scoped_ptr<TemplateURLService> model_a_;
  scoped_ptr<TestingProfile> profile_b_;
  scoped_ptr<TemplateURLService> model_b_;

  // Our dummy ChangeProcessor used to inspect changes pushed to Sync.
  TestChangeProcessor processor_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLServiceSyncTest);
};

}  // namespace

TEST_F(TemplateURLServiceSyncTest, SerializeDeserialize) {
  // Create a TemplateURL and convert it into a sync specific type.
  scoped_ptr<TemplateURL> turl(CreateTestTemplateURL());
  SyncData sync_data = TemplateURLService::CreateSyncDataFromTemplateURL(*turl);
  // Convert the specifics back to a TemplateURL.
  scoped_ptr<TemplateURL> deserialized(
      TemplateURLService::CreateTemplateURLFromSyncData(sync_data));
  EXPECT_TRUE(deserialized.get());
  // Ensure that the original and the deserialized TURLs are equal in values.
  AssertEquals(*turl, *deserialized);
}

TEST_F(TemplateURLServiceSyncTest, GetAllSyncDataBasic) {
  model()->Add(CreateTestTemplateURL("key1", "http://key1.com"));
  model()->Add(CreateTestTemplateURL("key2", "http://key2.com"));
  model()->Add(CreateTestTemplateURL("key3", "http://key3.com"));
  SyncDataList all_sync_data =
      model()->GetAllSyncData(syncable::SEARCH_ENGINES);

  EXPECT_EQ(3U, all_sync_data.size());

  for (SyncDataList::const_iterator iter = all_sync_data.begin();
      iter != all_sync_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    const TemplateURL* service_turl = model()->GetTemplateURLForGUID(guid);
    scoped_ptr<TemplateURL> deserialized(
        TemplateURLService::CreateTemplateURLFromSyncData(*iter));
    AssertEquals(*service_turl, *deserialized);
  }
}

TEST_F(TemplateURLServiceSyncTest, GetAllSyncDataNoExtensions) {
  model()->Add(CreateTestTemplateURL("key1", "http://key1.com"));
  model()->Add(CreateTestTemplateURL("key2", "http://key2.com"));
  model()->Add(CreateTestTemplateURL(
      "key3", "chrome-extension://blahblahblah"));
  SyncDataList all_sync_data =
      model()->GetAllSyncData(syncable::SEARCH_ENGINES);

  EXPECT_EQ(2U, all_sync_data.size());

  for (SyncDataList::const_iterator iter = all_sync_data.begin();
      iter != all_sync_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    const TemplateURL* service_turl = model()->GetTemplateURLForGUID(guid);
    scoped_ptr<TemplateURL> deserialized(
        TemplateURLService::CreateTemplateURLFromSyncData(*iter));
    AssertEquals(*service_turl, *deserialized);
  }
}

TEST_F(TemplateURLServiceSyncTest, GetAllSyncDataNoManagedEngines) {
  model()->Add(CreateTestTemplateURL("key1", "http://key1.com"));
  model()->Add(CreateTestTemplateURL("key2", "http://key2.com"));
  TemplateURL* managed_turl = CreateTestTemplateURL(
      "key3", "http://key3.com");
  managed_turl->set_created_by_policy(true);
  model()->Add(managed_turl);
  SyncDataList all_sync_data =
      model()->GetAllSyncData(syncable::SEARCH_ENGINES);

  EXPECT_EQ(2U, all_sync_data.size());

  for (SyncDataList::const_iterator iter = all_sync_data.begin();
      iter != all_sync_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    const TemplateURL* service_turl = model()->GetTemplateURLForGUID(guid);
    scoped_ptr<TemplateURL> deserialized(
        TemplateURLService::CreateTemplateURLFromSyncData(*iter));
    ASSERT_FALSE(service_turl->created_by_policy());
    AssertEquals(*service_turl, *deserialized);
  }
}

TEST_F(TemplateURLServiceSyncTest, UniquifyKeyword) {
  model()->Add(CreateTestTemplateURL("key1", "http://key1.com"));
  // Create a key that conflicts with something in the model.
  scoped_ptr<TemplateURL> turl(CreateTestTemplateURL("key1",
                                                     "http://new.com",
                                                     "xyz"));
  string16 new_keyword = model()->UniquifyKeyword(*turl);

  EXPECT_EQ(UTF8ToUTF16("new.com"), new_keyword);
  EXPECT_EQ(NULL, model()->GetTemplateURLForKeyword(new_keyword));

  turl->set_keyword(new_keyword);
  model()->Add(turl.release());
  // Test a second collision. This time it should be resolved by actually
  // modifying the original keyword, since the autogenerated keyword is already
  // used.
  turl.reset(CreateTestTemplateURL("key1", "http://new.com"));
  new_keyword = model()->UniquifyKeyword(*turl);

  EXPECT_EQ(UTF8ToUTF16("key1_"), new_keyword);
  EXPECT_EQ(NULL, model()->GetTemplateURLForKeyword(new_keyword));

  turl->set_keyword(new_keyword);
  model()->Add(turl.release());
  // Test a third collision. This should collide on both the autogenerated
  // keyword and the first uniquification attempt.
  turl.reset(CreateTestTemplateURL("key1", "http://new.com"));
  new_keyword = model()->UniquifyKeyword(*turl);

  EXPECT_EQ(UTF8ToUTF16("key1__"), new_keyword);
  EXPECT_EQ(NULL, model()->GetTemplateURLForKeyword(new_keyword));
}

TEST_F(TemplateURLServiceSyncTest, ResolveSyncKeywordConflict) {
  TemplateURL* original_turl =
      CreateTestTemplateURL("key1", "http://key1.com", std::string(), 9000);
  string16 original_turl_keyword = original_turl->keyword();
  model()->Add(original_turl);

  // Create a key that does not conflict with something in the model.
  scoped_ptr<TemplateURL> sync_turl(CreateTestTemplateURL("unique",
                                                          "http://new.com"));
  string16 sync_keyword = sync_turl->keyword();
  SyncChangeList changes;

  // No conflict, no TURLs changed, no changes.
  EXPECT_FALSE(model()->ResolveSyncKeywordConflict(sync_turl.get(), &changes));
  EXPECT_EQ(original_turl_keyword, original_turl->keyword());
  EXPECT_EQ(sync_keyword, sync_turl->keyword());
  EXPECT_EQ(0U, changes.size());

  // Change sync keyword to something that conflicts, and make it older.
  // Conflict, sync keyword is uniquified, and a SyncChange is added.
  sync_turl->set_keyword(original_turl->keyword());
  sync_turl->set_last_modified(Time::FromTimeT(8999));
  EXPECT_TRUE(model()->ResolveSyncKeywordConflict(sync_turl.get(), &changes));
  EXPECT_NE(sync_keyword, sync_turl->keyword());
  EXPECT_EQ(original_turl_keyword, original_turl->keyword());
  EXPECT_EQ(NULL, model()->GetTemplateURLForKeyword(sync_turl->keyword()));
  EXPECT_EQ(1U, changes.size());
  changes.clear();

  // Sync is newer. Original TemplateURL keyword is uniquified, no SyncChange
  // is added.
  sync_turl->set_keyword(original_turl->keyword());
  sync_keyword = sync_turl->keyword();
  sync_turl->set_last_modified(Time::FromTimeT(9001));
  EXPECT_TRUE(model()->ResolveSyncKeywordConflict(sync_turl.get(), &changes));
  EXPECT_EQ(sync_keyword, sync_turl->keyword());
  EXPECT_NE(original_turl_keyword, original_turl->keyword());
  EXPECT_EQ(NULL, model()->GetTemplateURLForKeyword(sync_turl->keyword()));
  EXPECT_EQ(0U, changes.size());

  // Equal times. Same result as above. Sync left alone, original uniquified so
  // sync_turl can fit.
  sync_turl->set_keyword(original_turl->keyword());
  sync_keyword = sync_turl->keyword();
  // Note that we have to reset original_turl's last_modified time as it was
  // modified above.
  original_turl->set_last_modified(Time::FromTimeT(9000));
  sync_turl->set_last_modified(Time::FromTimeT(9000));
  EXPECT_TRUE(model()->ResolveSyncKeywordConflict(sync_turl.get(), &changes));
  EXPECT_EQ(sync_keyword, sync_turl->keyword());
  EXPECT_NE(original_turl_keyword, original_turl->keyword());
  EXPECT_EQ(NULL, model()->GetTemplateURLForKeyword(sync_turl->keyword()));
  EXPECT_EQ(0U, changes.size());

  // Sync is newer, but original TemplateURL is created by policy, so it wins.
  // Sync keyword is uniquified, and a SyncChange is added.
  original_turl_keyword = original_turl->keyword();
  sync_turl->set_keyword(original_turl->keyword());
  sync_turl->set_last_modified(Time::FromTimeT(9999));
  original_turl->set_created_by_policy(true);
  EXPECT_TRUE(model()->ResolveSyncKeywordConflict(sync_turl.get(), &changes));
  EXPECT_NE(sync_keyword, sync_turl->keyword());
  EXPECT_EQ(original_turl_keyword, original_turl->keyword());
  EXPECT_EQ(NULL, model()->GetTemplateURLForKeyword(sync_turl->keyword()));
  EXPECT_EQ(1U, changes.size());
  changes.clear();
}

TEST_F(TemplateURLServiceSyncTest, FindDuplicateOfSyncTemplateURL) {
  TemplateURL* original_turl =
      CreateTestTemplateURL("key1", "http://key1.com");
  model()->Add(original_turl);

  // No matches at all.
  scoped_ptr<TemplateURL> sync_turl(
      CreateTestTemplateURL("key2", "http://key2.com"));
  EXPECT_EQ(NULL, model()->FindDuplicateOfSyncTemplateURL(*sync_turl));

  // URL matches, but not keyword. No dupe.
  sync_turl->SetURL("http://key1.com", 0 , 0);
  EXPECT_EQ(NULL, model()->FindDuplicateOfSyncTemplateURL(*sync_turl));

  // Keyword matches, but not URL. No dupe.
  sync_turl->SetURL("http://key2.com", 0 , 0);
  sync_turl->set_keyword(UTF8ToUTF16("key1"));
  EXPECT_EQ(NULL, model()->FindDuplicateOfSyncTemplateURL(*sync_turl));

  // Duplicate.
  sync_turl->SetURL("http://key1.com", 0 , 0);
  const TemplateURL* dupe_turl =
      model()->FindDuplicateOfSyncTemplateURL(*sync_turl);
  ASSERT_TRUE(dupe_turl);
  EXPECT_EQ(dupe_turl->keyword(), sync_turl->keyword());
  EXPECT_EQ(dupe_turl->url()->url(), sync_turl->url()->url());
}

TEST_F(TemplateURLServiceSyncTest, MergeSyncAndLocalURLDuplicates) {
  TemplateURL* original_turl =
      CreateTestTemplateURL("key1", "http://key1.com", std::string(), 9000);
  model()->Add(original_turl);
  TemplateURL* sync_turl =
      CreateTestTemplateURL("key1", "http://key1.com", std::string(), 9001);
  SyncChangeList changes;

  // The sync TemplateURL is newer. It should replace the original TemplateURL.
  // Note that MergeSyncAndLocalURLDuplicates takes ownership of sync_turl.
  model()->MergeSyncAndLocalURLDuplicates(sync_turl, original_turl, &changes);
  const TemplateURL* result =
      model()->GetTemplateURLForKeyword(UTF8ToUTF16("key1"));
  ASSERT_TRUE(result);
  EXPECT_EQ(9001, result->last_modified().ToTimeT());
  EXPECT_EQ(0U, changes.size());

  // The sync TemplateURL is older. The existing TemplateURL should win and a
  // SyncChange should be added to the list.
  TemplateURL* sync_turl2 =
      CreateTestTemplateURL("key1", "http://key1.com", std::string(), 8999);
  model()->MergeSyncAndLocalURLDuplicates(sync_turl2, sync_turl, &changes);
  result = model()->GetTemplateURLForKeyword(UTF8ToUTF16("key1"));
  ASSERT_TRUE(result);
  EXPECT_EQ(9001, result->last_modified().ToTimeT());
  EXPECT_EQ(1U, changes.size());
}

TEST_F(TemplateURLServiceSyncTest, StartSyncEmpty) {
  model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      SyncDataList(),  // Empty.
      processor());

  EXPECT_EQ(0U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());
  EXPECT_EQ(0, processor()->change_list_size());
}

TEST_F(TemplateURLServiceSyncTest, MergeIntoEmpty) {
  SyncDataList initial_data = CreateInitialSyncData();

  model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      initial_data,
      processor());

  EXPECT_EQ(3U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());
  // We expect the model to have accepted all of the initial sync data. Search
  // through the model using the GUIDs to ensure that they're present.
  for (SyncDataList::const_iterator iter = initial_data.begin();
      iter != initial_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    EXPECT_TRUE(model()->GetTemplateURLForGUID(guid));
  }

  EXPECT_EQ(0, processor()->change_list_size());
}

TEST_F(TemplateURLServiceSyncTest, MergeInAllNewData) {
  model()->Add(CreateTestTemplateURL(
      "google.com", "http://google.com", "abc"));
  model()->Add(CreateTestTemplateURL("yahoo.com", "http://yahoo.com", "def"));
  model()->Add(CreateTestTemplateURL("bing.com", "http://bing.com", "xyz"));
  SyncDataList initial_data = CreateInitialSyncData();

  model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      initial_data,
      processor());

  EXPECT_EQ(6U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());
  // We expect the model to have accepted all of the initial sync data. Search
  // through the model using the GUIDs to ensure that they're present.
  for (SyncDataList::const_iterator iter = initial_data.begin();
      iter != initial_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    EXPECT_TRUE(model()->GetTemplateURLForGUID(guid));
  }
  // All the original TemplateURLs should also remain in the model.
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(UTF8ToUTF16("google.com")));
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(UTF8ToUTF16("yahoo.com")));
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(UTF8ToUTF16("bing.com")));
  // Ensure that Sync received the expected changes.
  EXPECT_EQ(3, processor()->change_list_size());
  EXPECT_TRUE(processor()->ContainsGUID("abc"));
  EXPECT_TRUE(processor()->ContainsGUID("def"));
  EXPECT_TRUE(processor()->ContainsGUID("xyz"));
}

TEST_F(TemplateURLServiceSyncTest, MergeSyncIsTheSame) {
  // The local data is the same as the sync data merged in. i.e. - There have
  // been no changes since the last time we synced. Even the last_modified
  // timestamps are the same.
  SyncDataList initial_data = CreateInitialSyncData();
  for (SyncDataList::const_iterator iter = initial_data.begin();
      iter != initial_data.end(); ++iter) {
    TemplateURL* converted =
        TemplateURLService::CreateTemplateURLFromSyncData(*iter);
    model()->Add(converted);
  }

  model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      initial_data,
      processor());

  EXPECT_EQ(3U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());
  for (SyncDataList::const_iterator iter = initial_data.begin();
      iter != initial_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    EXPECT_TRUE(model()->GetTemplateURLForGUID(guid));
  }
  EXPECT_EQ(0, processor()->change_list_size());
}

TEST_F(TemplateURLServiceSyncTest, MergeUpdateFromSync) {
  // The local data is the same as the sync data merged in, but timestamps have
  // changed. Ensure the right fields are merged in.
  SyncDataList initial_data;
  TemplateURL* turl1 = CreateTestTemplateURL(
      "google.com", "http://google.com", "abc", 9000);
  model()->Add(turl1);
  TemplateURL* turl2 = CreateTestTemplateURL(
      "bing.com", "http://bing.com", "xyz", 9000);
  model()->Add(turl2);

  TemplateURL turl1_newer(*turl1);
  turl1_newer.set_last_modified(Time::FromTimeT(9999));
  turl1_newer.SetURL("http://google.ca", 0, 0);
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURL(turl1_newer));

  TemplateURL turl2_older(*turl2);
  turl2_older.set_last_modified(Time::FromTimeT(8888));
  turl2_older.SetURL("http://bing.ca", 0, 0);
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURL(turl2_older));

  model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      initial_data,
      processor());

  // Both were local updates, so we expect the same count.
  EXPECT_EQ(2U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());

  // Check that the first replaced the initial Google TemplateURL.
  EXPECT_EQ(turl1, model()->GetTemplateURLForGUID("abc"));
  EXPECT_EQ("http://google.ca", turl1->url()->url());

  // Check that the second produced an upstream update to the Bing TemplateURL.
  EXPECT_EQ(1, processor()->change_list_size());
  ASSERT_TRUE(processor()->ContainsGUID("xyz"));
  SyncChange change = processor()->GetChangeByGUID("xyz");
  EXPECT_TRUE(change.change_type() == SyncChange::ACTION_UPDATE);
  EXPECT_EQ("http://bing.com", GetURL(change.sync_data()));
}

TEST_F(TemplateURLServiceSyncTest, MergeAddFromOlderSyncData) {
  // GUIDs all differ, so this is data to be added from Sync, but the timestamps
  // from Sync are older. Set up the local data so that one is a dupe, one has a
  // conflicting keyword, and the last has no conflicts (a clean ADD).
  SyncDataList initial_data = CreateInitialSyncData();
  TemplateURL* turl = TemplateURLService::CreateTemplateURLFromSyncData(
      initial_data.at(0));
  ASSERT_TRUE(turl);
  turl->set_sync_guid("aaa");
  turl->set_last_modified(Time::FromTimeT(100));
  model()->Add(turl);  // dupe

  turl = TemplateURLService::CreateTemplateURLFromSyncData(
      initial_data.at(1));
  ASSERT_TRUE(turl);
  turl->SetURL("http://expected.com", 0, 0);
  turl->set_sync_guid("bbb");
  turl->set_last_modified(Time::FromTimeT(100));
  model()->Add(turl);  // keyword conflict

  model()->Add(CreateTestTemplateURL(
      "unique", "http://unique.com", "ccc"));  // add

  model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      initial_data,
      processor());

  // The dupe results in a merge. The other two should be added to the model.
  EXPECT_EQ(5U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());

  // The key1 duplicate results in the local copy winning. Ensure that Sync's
  // copy was not added, and the local copy is pushed upstream to Sync as an
  // update. The local copy should have received the sync data's GUID.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key1"));
  // Check changes for the UPDATE.
  ASSERT_TRUE(processor()->ContainsGUID("key1"));
  SyncChange key1_change = processor()->GetChangeByGUID("key1");
  EXPECT_EQ(SyncChange::ACTION_UPDATE, key1_change.change_type());
  EXPECT_FALSE(model()->GetTemplateURLForGUID("aaa"));

  // The key2 keyword conflict results in the local copy winning, so ensure it
  // retains the original keyword, and that an update to the sync copy is pushed
  // upstream to Sync. Both TemplateURLs should be found locally, however.
  const TemplateURL* key2 = model()->GetTemplateURLForGUID("bbb");
  EXPECT_TRUE(key2);
  EXPECT_EQ(UTF8ToUTF16("key2"), key2->keyword());
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key2"));
  // Check changes for the UPDATE.
  ASSERT_TRUE(processor()->ContainsGUID("key2"));
  SyncChange key2_change = processor()->GetChangeByGUID("key2");
  EXPECT_EQ(SyncChange::ACTION_UPDATE, key2_change.change_type());
  EXPECT_EQ("key2.com", GetKeyword(key2_change.sync_data()));

  // The last TemplateURL should have had no conflicts and was just added. It
  // should not have replaced the third local TemplateURL.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("ccc"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key3"));

  // Two UPDATEs and two ADDs.
  EXPECT_EQ(4, processor()->change_list_size());
  // Two ADDs should be pushed up to Sync.
  ASSERT_TRUE(processor()->ContainsGUID("bbb"));
  EXPECT_EQ(SyncChange::ACTION_ADD,
            processor()->GetChangeByGUID("bbb").change_type());
  ASSERT_TRUE(processor()->ContainsGUID("ccc"));
  EXPECT_EQ(SyncChange::ACTION_ADD,
            processor()->GetChangeByGUID("ccc").change_type());
}

TEST_F(TemplateURLServiceSyncTest, MergeAddFromNewerSyncData) {
  // GUIDs all differ, so this is data to be added from Sync, but the timestamps
  // from Sync are newer. Set up the local data so that one is a dupe, one has a
  // conflicting keyword, and the last has no conflicts (a clean ADD).
  SyncDataList initial_data = CreateInitialSyncData();
  TemplateURL* turl = TemplateURLService::CreateTemplateURLFromSyncData(
      initial_data.at(0));
  ASSERT_TRUE(turl);
  turl->set_sync_guid("aaa");
  turl->set_last_modified(Time::FromTimeT(10));
  model()->Add(turl);  // dupe

  turl = TemplateURLService::CreateTemplateURLFromSyncData(
      initial_data.at(1));
  ASSERT_TRUE(turl);
  turl->SetURL("http://expected.com", 0, 0);
  turl->set_sync_guid("bbb");
  turl->set_last_modified(Time::FromTimeT(10));
  model()->Add(turl);  // keyword conflict

  model()->Add(CreateTestTemplateURL(
      "unique", "http://unique.com", "ccc", 10));  // add

  model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      initial_data,
      processor());

  // The dupe results in a merge. The other two should be added to the model.
  EXPECT_EQ(5U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());

  // The key1 duplicate results in Sync's copy winning. Ensure that Sync's
  // copy replaced the local copy.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key1"));
  EXPECT_FALSE(model()->GetTemplateURLForGUID("aaa"));

  // The key2 keyword conflict results in Sync's copy winning, so ensure it
  // retains the original keyword. The local copy should get a uniquified
  // keyword. Both TemplateURLs should be found locally.
  const TemplateURL* key2_sync = model()->GetTemplateURLForGUID("key2");
  EXPECT_TRUE(key2_sync);
  EXPECT_EQ(UTF8ToUTF16("key2"), key2_sync->keyword());
  const TemplateURL* key2_local = model()->GetTemplateURLForGUID("bbb");
  EXPECT_TRUE(key2_local);
  EXPECT_EQ(UTF8ToUTF16("expected.com"), key2_local->keyword());

  // The last TemplateURL should have had no conflicts and was just added. It
  // should not have replaced the third local TemplateURL.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("ccc"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key3"));

  // Two ADDs.
  EXPECT_EQ(2, processor()->change_list_size());
  // Two ADDs should be pushed up to Sync.
  ASSERT_TRUE(processor()->ContainsGUID("bbb"));
  EXPECT_EQ(SyncChange::ACTION_ADD,
            processor()->GetChangeByGUID("bbb").change_type());
  ASSERT_TRUE(processor()->ContainsGUID("ccc"));
  EXPECT_EQ(SyncChange::ACTION_ADD,
            processor()->GetChangeByGUID("ccc").change_type());
}

TEST_F(TemplateURLServiceSyncTest, ProcessChangesEmptyModel) {
  // We initially have no data.
  model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      SyncDataList(),
      processor());

  // Set up a bunch of ADDs.
  SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      SyncChange::ACTION_ADD,
      CreateTestTemplateURL("key1", "http://key1.com", "key1")));
  changes.push_back(CreateTestSyncChange(
      SyncChange::ACTION_ADD,
      CreateTestTemplateURL("key2", "http://key2.com", "key2")));
  changes.push_back(CreateTestSyncChange(
      SyncChange::ACTION_ADD,
      CreateTestTemplateURL("key3", "http://key3.com", "key3")));

  model()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_EQ(3U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());
  EXPECT_EQ(0, processor()->change_list_size());
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key1"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key2"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key3"));
}

TEST_F(TemplateURLServiceSyncTest, ProcessChangesNoConflicts) {
  model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      CreateInitialSyncData(),
      processor());

  // Process different types of changes, without conflicts.
  SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      SyncChange::ACTION_ADD,
      CreateTestTemplateURL("key4", "http://key4.com", "key4")));
  changes.push_back(CreateTestSyncChange(
      SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL("newkeyword", "http://new.com", "key2")));
  changes.push_back(CreateTestSyncChange(
      SyncChange::ACTION_DELETE,
      CreateTestTemplateURL("key3", "http://key3.com", "key3")));

  model()->ProcessSyncChanges(FROM_HERE, changes);

  // Add one, remove one, update one, so the number shouldn't change.
  EXPECT_EQ(3U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());
  EXPECT_EQ(0, processor()->change_list_size());
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key1"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key2"));
  const TemplateURL* turl = model()->GetTemplateURLForGUID("key2");
  EXPECT_TRUE(turl);
  EXPECT_EQ(UTF8ToUTF16("newkeyword"), turl->keyword());
  EXPECT_EQ("http://new.com", turl->url()->url());
  EXPECT_FALSE(model()->GetTemplateURLForGUID("key3"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key4"));
}

TEST_F(TemplateURLServiceSyncTest, ProcessChangesWithConflictsSyncWins) {
  model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      CreateInitialSyncData(),
      processor());

  // Process different types of changes, with conflicts. Note that all this data
  // has a newer timestamp, so Sync will win in these scenarios.
  SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      SyncChange::ACTION_ADD,
      CreateTestTemplateURL("key2", "http://new.com", "aaa")));
  changes.push_back(CreateTestSyncChange(
      SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL("key3", "http://key3.com", "key1")));

  model()->ProcessSyncChanges(FROM_HERE, changes);

  // Add one, update one, so we're up to 4.
  EXPECT_EQ(4U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());
  // Sync is always newer here, so it should always win (i.e. - local changes,
  // nothing pushed to Sync).
  EXPECT_EQ(0, processor()->change_list_size());

  // aaa conflicts with key2 and wins, forcing key2's keyword to update.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("aaa"));
  EXPECT_EQ(model()->GetTemplateURLForGUID("aaa"),
            model()->GetTemplateURLForKeyword(UTF8ToUTF16("key2")));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key2"));
  EXPECT_EQ(model()->GetTemplateURLForGUID("key2"),
            model()->GetTemplateURLForKeyword(UTF8ToUTF16("key2.com")));
  // key1 update conflicts with key3 and wins, forcing key3's keyword to update.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key1"));
  EXPECT_EQ(model()->GetTemplateURLForGUID("key1"),
            model()->GetTemplateURLForKeyword(UTF8ToUTF16("key3")));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key3"));
  EXPECT_EQ(model()->GetTemplateURLForGUID("key3"),
            model()->GetTemplateURLForKeyword(UTF8ToUTF16("key3.com")));
}

TEST_F(TemplateURLServiceSyncTest, ProcessChangesWithConflictsLocalWins) {
  model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      CreateInitialSyncData(),
      processor());

  // Process different types of changes, with conflicts. Note that all this data
  // has an older timestamp, so the local data will win in these scenarios.
  SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      SyncChange::ACTION_ADD,
      CreateTestTemplateURL("key2", "http://new.com", "aaa", 10)));
  changes.push_back(CreateTestSyncChange(
      SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL("key3", "http://key3.com", "key1", 10)));

  model()->ProcessSyncChanges(FROM_HERE, changes);

  // Add one, update one, so we're up to 4.
  EXPECT_EQ(4U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());
  // Local data wins twice so two updates are pushed up to Sync.
  EXPECT_EQ(2, processor()->change_list_size());

  // aaa conflicts with key2 and loses, forcing it's keyword to update.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("aaa"));
  EXPECT_EQ(model()->GetTemplateURLForGUID("aaa"),
            model()->GetTemplateURLForKeyword(UTF8ToUTF16("new.com")));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key2"));
  EXPECT_EQ(model()->GetTemplateURLForGUID("key2"),
            model()->GetTemplateURLForKeyword(UTF8ToUTF16("key2")));
  // key1 update conflicts with key3 and loses, forcing key1's keyword to
  // update.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key1"));
  EXPECT_EQ(model()->GetTemplateURLForGUID("key1"),
            model()->GetTemplateURLForKeyword(UTF8ToUTF16("key3.com")));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key3"));
  EXPECT_EQ(model()->GetTemplateURLForGUID("key3"),
            model()->GetTemplateURLForKeyword(UTF8ToUTF16("key3")));

  ASSERT_TRUE(processor()->ContainsGUID("aaa"));
  EXPECT_EQ(SyncChange::ACTION_UPDATE,
            processor()->GetChangeByGUID("aaa").change_type());
  ASSERT_TRUE(processor()->ContainsGUID("key1"));
  EXPECT_EQ(SyncChange::ACTION_UPDATE,
            processor()->GetChangeByGUID("key1").change_type());
}

TEST_F(TemplateURLServiceSyncTest, ProcessTemplateURLChange) {
  // Ensure that ProcessTemplateURLChange is called and pushes the correct
  // changes to Sync whenever local changes are made to TemplateURLs.
  model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      CreateInitialSyncData(),
      processor());

  // Add a new search engine.
  TemplateURL* new_turl =
      CreateTestTemplateURL("baidu", "http://baidu.cn", "new");
  model()->Add(new_turl);
  EXPECT_EQ(1, processor()->change_list_size());
  ASSERT_TRUE(processor()->ContainsGUID("new"));
  SyncChange change = processor()->GetChangeByGUID("new");
  EXPECT_EQ(SyncChange::ACTION_ADD, change.change_type());
  EXPECT_EQ("baidu", GetKeyword(change.sync_data()));
  EXPECT_EQ("http://baidu.cn", GetURL(change.sync_data()));

  // Change a keyword.
  const TemplateURL* existing_turl = model()->GetTemplateURLForGUID("key1");
  model()->ResetTemplateURL(existing_turl,
                            existing_turl->short_name(),
                            UTF8ToUTF16("k"),
                            existing_turl->url()->url());
  EXPECT_EQ(1, processor()->change_list_size());
  ASSERT_TRUE(processor()->ContainsGUID("key1"));
  change = processor()->GetChangeByGUID("key1");
  EXPECT_EQ(SyncChange::ACTION_UPDATE, change.change_type());
  EXPECT_EQ("k", GetKeyword(change.sync_data()));

  // Remove an existing search engine.
  existing_turl = model()->GetTemplateURLForGUID("key2");
  model()->Remove(existing_turl);
  EXPECT_EQ(1, processor()->change_list_size());
  ASSERT_TRUE(processor()->ContainsGUID("key2"));
  change = processor()->GetChangeByGUID("key2");
  EXPECT_EQ(SyncChange::ACTION_DELETE, change.change_type());
}

TEST_F(TemplateURLServiceSyncTest, MergeTwoClientsBasic) {
  // Start off B with some empty data.
  model_b()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      CreateInitialSyncData(),
      processor());

  // Merge A and B. All of B's data should transfer over to A, which initially
  // has no data.
  model_a()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      model_b()->GetAllSyncData(syncable::SEARCH_ENGINES),
      model_b());

  // They should be consistent.
  AssertEquals(model_a()->GetAllSyncData(syncable::SEARCH_ENGINES),
               model_b()->GetAllSyncData(syncable::SEARCH_ENGINES));
}

TEST_F(TemplateURLServiceSyncTest, MergeTwoClientsDupesAndConflicts) {
  // Start off B with some empty data.
  model_b()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      CreateInitialSyncData(),
      processor());

  // Set up A so we have some interesting duplicates and conflicts.
  model_a()->Add(CreateTestTemplateURL(
      "key4", "http://key4.com", "key4"));  // Added
  model_a()->Add(CreateTestTemplateURL(
      "key2", "http://key2.com", "key2"));  // Merge - Copy of key2.
  model_a()->Add(CreateTestTemplateURL(
      "key3", "http://key3.com", "key5", 10));  // Merge - Dupe of key3.
  model_a()->Add(CreateTestTemplateURL(
      "key1", "http://key6.com", "key6", 10));  // Keyword conflict with key1

  // Merge A and B.
  model_a()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      model_b()->GetAllSyncData(syncable::SEARCH_ENGINES),
      model_b());

  // They should be consistent.
  AssertEquals(model_a()->GetAllSyncData(syncable::SEARCH_ENGINES),
               model_b()->GetAllSyncData(syncable::SEARCH_ENGINES));
}

TEST_F(TemplateURLServiceSyncTest, StopSyncing) {
  SyncError error = model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      CreateInitialSyncData(),
      processor());
  ASSERT_FALSE(error.IsSet());
  model()->StopSyncing(syncable::SEARCH_ENGINES);

  SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL("newkeyword", "http://new.com", "key2")));
  error = model()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_TRUE(error.IsSet());

  // Ensure that the sync changes were not accepted.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key2"));
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(UTF8ToUTF16("newkeyword")));
}

TEST_F(TemplateURLServiceSyncTest, SyncErrorOnInitialSync) {
  processor()->set_erroneous(true);
  SyncError error = model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      CreateInitialSyncData(),
      processor());
  EXPECT_TRUE(error.IsSet());

  // Ensure that if the initial merge was erroneous, then subsequence attempts
  // to push data into the local model are rejected, since the model was never
  // successfully associated with Sync in the first place.
  SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL("newkeyword", "http://new.com", "key2")));
  processor()->set_erroneous(false);
  error = model()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_TRUE(error.IsSet());

  // Ensure that the sync changes were not accepted.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("key2"));
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(UTF8ToUTF16("newkeyword")));
}

TEST_F(TemplateURLServiceSyncTest, SyncErrorOnLaterSync) {
  // Ensure that if the SyncProcessor succeeds in the initial merge, but fails
  // in future ProcessSyncChanges, we still return an error.
  SyncError error = model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      CreateInitialSyncData(),
      processor());
  ASSERT_FALSE(error.IsSet());

  SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL("newkeyword", "http://new.com", "key2")));
  processor()->set_erroneous(true);
  error = model()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_TRUE(error.IsSet());
}

TEST_F(TemplateURLServiceSyncTest, MergeTwiceWithSameSyncData) {
  // Ensure that a second merge with the same data as the first does not
  // actually update the local data.
  SyncDataList initial_data;
  initial_data.push_back(CreateInitialSyncData().at(0));
  TemplateURL* turl = TemplateURLService::CreateTemplateURLFromSyncData(
      initial_data.at(0));
  ASSERT_TRUE(turl);
  turl->set_last_modified(Time::FromTimeT(10));  // earlier
  model()->Add(turl);

  SyncError error = model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      initial_data,
      processor());
  ASSERT_FALSE(error.IsSet());

  // We should have updated the original TemplateURL with Sync's version.
  // Keep a copy of it so we can compare it after we re-merge.
  ASSERT_TRUE(model()->GetTemplateURLForGUID("key1"));
  TemplateURL updated_turl(*model()->GetTemplateURLForGUID("key1"));
  EXPECT_EQ(Time::FromTimeT(90), updated_turl.last_modified());

  // Modify a single field of the initial data. This should not be updated in
  // the second merge, as the last_modified timestamp remains the same.
  scoped_ptr<TemplateURL> temp_turl(
      TemplateURLService::CreateTemplateURLFromSyncData(initial_data.at(0)));
  temp_turl->set_short_name(UTF8ToUTF16("SomethingDifferent"));
  initial_data.clear();
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURL(*temp_turl));

  // Remerge the data again. This simulates shutting down and syncing again
  // at a different time, but the cloud data has not changed.
  model()->StopSyncing(syncable::SEARCH_ENGINES);
  error = model()->MergeDataAndStartSyncing(syncable::SEARCH_ENGINES,
                                            initial_data,
                                            processor());
  ASSERT_FALSE(error.IsSet());

  // Check that the TemplateURL was not modified.
  const TemplateURL* reupdated_turl = model()->GetTemplateURLForGUID("key1");
  ASSERT_TRUE(reupdated_turl);
  AssertEquals(updated_turl, *reupdated_turl);
}

TEST_F(TemplateURLServiceSyncTest, SyncedDefaultGUIDArrivesFirst) {
  SyncDataList initial_data = CreateInitialSyncData();
  model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      initial_data,
      processor());
  model()->SetDefaultSearchProvider(model()->GetTemplateURLForGUID("key2"));

  EXPECT_EQ(3U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());
  const TemplateURL* default_search = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);

  // Change kSyncedDefaultSearchProviderGUID to a GUID that does not exist in
  // the model yet. Ensure that the default has not changed in any way.
  profile_a_->GetTestingPrefService()->SetString(
      prefs::kSyncedDefaultSearchProviderGUID,
      "newdefault");

  ASSERT_EQ(default_search, model()->GetDefaultSearchProvider());

  // Bring in a random new search engine with a different GUID. Ensure that
  // it doesn't change the default.
  SyncChangeList changes1;
  changes1.push_back(CreateTestSyncChange(
      SyncChange::ACTION_ADD,
      CreateTestTemplateURL("random", "http://random.com", "random")));
  model()->ProcessSyncChanges(FROM_HERE, changes1);

  EXPECT_EQ(4U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());
  ASSERT_EQ(default_search, model()->GetDefaultSearchProvider());

  // Finally, bring in the expected entry with the right GUID. Ensure that
  // the default has changed to the new search engine.
  SyncChangeList changes2;
  changes2.push_back(CreateTestSyncChange(
      SyncChange::ACTION_ADD,
      CreateTestTemplateURL("new", "http://new.com", "newdefault")));
  model()->ProcessSyncChanges(FROM_HERE, changes2);

  EXPECT_EQ(5U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());
  ASSERT_NE(default_search, model()->GetDefaultSearchProvider());
  ASSERT_EQ("newdefault", model()->GetDefaultSearchProvider()->sync_guid());
}

TEST_F(TemplateURLServiceSyncTest, SyncedDefaultArrivesAfterStartup) {
  // Start with the default set to something in the model before we start
  // syncing.
  model()->Add(CreateTestTemplateURL(
      "what", "http://thewhat.com", "initdefault"));
  model()->SetDefaultSearchProvider(
      model()->GetTemplateURLForGUID("initdefault"));

  const TemplateURL* default_search = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);

  // Set kSyncedDefaultSearchProviderGUID to something that is not yet in
  // the model but is expected in the initial sync. Ensure that this doesn't
  // change our default since we're not quite syncing yet.
  profile_a_->GetTestingPrefService()->SetString(
      prefs::kSyncedDefaultSearchProviderGUID,
      "key2");

  EXPECT_EQ(default_search, model()->GetDefaultSearchProvider());

  // Now sync the initial data, which will include the search engine entry
  // destined to become the new default.
  SyncDataList initial_data = CreateInitialSyncData();
  model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      initial_data,
      processor());

  // Ensure that the new default has been set.
  EXPECT_EQ(4U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());
  ASSERT_NE(default_search, model()->GetDefaultSearchProvider());
  ASSERT_EQ("key2", model()->GetDefaultSearchProvider()->sync_guid());
}

TEST_F(TemplateURLServiceSyncTest, SyncedDefaultAlreadySetOnStartup) {
  // Start with the default set to something in the model before we start
  // syncing.
  const char kGUID[] = "initdefault";
  model()->Add(CreateTestTemplateURL("what", "http://thewhat.com", kGUID));
  model()->SetDefaultSearchProvider(model()->GetTemplateURLForGUID(kGUID));

  const TemplateURL* default_search = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);

  // Set kSyncedDefaultSearchProviderGUID to the current default.
  profile_a_->GetTestingPrefService()->SetString(
      prefs::kSyncedDefaultSearchProviderGUID,
      kGUID);

  EXPECT_EQ(default_search, model()->GetDefaultSearchProvider());

  // Now sync the initial data.
  SyncDataList initial_data = CreateInitialSyncData();
  model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      initial_data,
      processor());

  // Ensure that the new entries were added and the default has not changed.
  EXPECT_EQ(4U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());
  ASSERT_EQ(default_search, model()->GetDefaultSearchProvider());
}

TEST_F(TemplateURLServiceSyncTest, SyncWithManagedDefaultSearch) {
  // First start off with a few entries and make sure we can set an unmanaged
  // default search provider.
  SyncDataList initial_data = CreateInitialSyncData();
  model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      initial_data,
      processor());
  model()->SetDefaultSearchProvider(model()->GetTemplateURLForGUID("key2"));

  EXPECT_EQ(3U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());
  ASSERT_FALSE(model()->is_default_search_managed());
  ASSERT_TRUE(model()->GetDefaultSearchProvider());

  // Change the default search provider to a managed one.
  const char kName[] = "manageddefault";
  const char kSearchURL[] = "http://manageddefault.com/search?t={searchTerms}";
  const char kIconURL[] = "http://manageddefault.com/icon.jpg";
  const char kEncodings[] = "UTF-16;UTF-32";
  SetManagedDefaultSearchPreferences(model(), profile_a_.get(), true, kName,
                                     kSearchURL, "", kIconURL, kEncodings, "");
  const TemplateURL* dsp_turl = model()->GetDefaultSearchProvider();

  EXPECT_TRUE(model()->is_default_search_managed());

  // Add a new entry from Sync. It should still sync in despite the default
  // being managed.
  SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      SyncChange::ACTION_ADD,
      CreateTestTemplateURL("newkeyword", "http://new.com", "newdefault")));
  model()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_EQ(4U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());

  // Change kSyncedDefaultSearchProviderGUID to point to the new entry and
  // ensure that the DSP remains managed.
  profile_a_->GetTestingPrefService()->SetString(
      prefs::kSyncedDefaultSearchProviderGUID,
      "newdefault");

  EXPECT_EQ(dsp_turl, model()->GetDefaultSearchProvider());
  EXPECT_TRUE(model()->is_default_search_managed());

  // Go unmanaged. Ensure that the DSP changes to the expected pending entry
  // from Sync.
  const TemplateURL* expected_default =
      model()->GetTemplateURLForGUID("newdefault");
  RemoveManagedDefaultSearchPreferences(model(), profile_a_.get());

  EXPECT_EQ(expected_default, model()->GetDefaultSearchProvider());
}

TEST_F(TemplateURLServiceSyncTest, SyncMergeDeletesDefault) {
  // If the value from Sync is a duplicate of the local default and is newer, it
  // should safely replace the local value and set as the new default.
  TemplateURL* default_turl =
      CreateTestTemplateURL("key1", "http://key1.com", "whateverguid", 10);
  model()->Add(default_turl);
  model()->SetDefaultSearchProvider(default_turl);

  // The key1 entry should be a duplicate of the default.
  model()->MergeDataAndStartSyncing(
      syncable::SEARCH_ENGINES,
      CreateInitialSyncData(),
      processor());

  EXPECT_EQ(3U, model()->GetAllSyncData(syncable::SEARCH_ENGINES).size());
  EXPECT_FALSE(model()->GetTemplateURLForGUID("whateverguid"));
  EXPECT_EQ(model()->GetDefaultSearchProvider(),
            model()->GetTemplateURLForGUID("key1"));
}
