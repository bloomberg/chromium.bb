// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_store.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "components/dom_distiller/core/article_entry.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using sync_pb::EntitySpecifics;
using syncer::ModelType;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncChangeProcessor;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using syncer::SyncErrorFactory;
using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;

namespace dom_distiller {

namespace {

const ModelType kDomDistillerModelType = syncer::ARTICLES;

typedef base::hash_map<std::string, ArticleEntry> EntryMap;

void AddEntry(const ArticleEntry& e, EntryMap* map) {
  (*map)[e.entry_id()] = e;
}

std::vector<ArticleEntry> EntryMapToList(const EntryMap& entries) {
  std::vector<ArticleEntry> entry_list;
  for (EntryMap::const_iterator it = entries.begin(); it != entries.end();
       ++it) {
    entry_list.push_back(it->second);
  }
  return entry_list;
}

class FakeDB : public DomDistillerDatabaseInterface {
  typedef base::Callback<void(bool)> Callback;

 public:
  explicit FakeDB(EntryMap* db) : db_(db) {}

  virtual void Init(
      const base::FilePath& database_dir,
      DomDistillerDatabaseInterface::InitCallback callback) OVERRIDE {
    dir_ = database_dir;
    init_callback_ = callback;
  }

  virtual void SaveEntries(
      scoped_ptr<EntryVector> entries_to_save,
      DomDistillerDatabaseInterface::SaveCallback callback) OVERRIDE {
    for (EntryVector::iterator it = entries_to_save->begin();
         it != entries_to_save->end();
         ++it) {
      (*db_)[it->entry_id()] = *it;
    }
    save_callback_ = callback;
  }

  virtual void LoadEntries(
      DomDistillerDatabaseInterface::LoadCallback callback) OVERRIDE {
    scoped_ptr<EntryVector> entries(new EntryVector());
    for (EntryMap::iterator it = db_->begin(); it != db_->end(); ++it) {
      entries->push_back(it->second);
    }
    load_callback_ =
        base::Bind(RunLoadCallback, callback, base::Passed(&entries));
  }

  base::FilePath& GetDirectory() { return dir_; }

  void InitCallback(bool success) {
    init_callback_.Run(success);
    init_callback_.Reset();
  }

  void LoadCallback(bool success) {
    load_callback_.Run(success);
    load_callback_.Reset();
  }

  void SaveCallback(bool success) {
    save_callback_.Run(success);
    save_callback_.Reset();
  }

 private:
  static void RunLoadCallback(
      DomDistillerDatabaseInterface::LoadCallback callback,
      scoped_ptr<EntryVector> entries,
      bool success) {
    callback.Run(success, entries.Pass());
  }

  base::FilePath dir_;
  EntryMap* db_;

  Callback init_callback_;
  Callback load_callback_;
  Callback save_callback_;
};

class FakeSyncErrorFactory : public syncer::SyncErrorFactory {
 public:
  virtual syncer::SyncError CreateAndUploadError(
      const tracked_objects::Location& location,
      const std::string& message) OVERRIDE {
    return syncer::SyncError();
  }
};

class FakeSyncChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  FakeSyncChangeProcessor(EntryMap* model) : model_(model) {}

  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const
      OVERRIDE {
    ADD_FAILURE() << "FakeSyncChangeProcessor::GetAllSyncData not implemented.";
    return syncer::SyncDataList();
  }

  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location&,
      const syncer::SyncChangeList& changes) OVERRIDE {
    for (SyncChangeList::const_iterator it = changes.begin();
         it != changes.end();
         ++it) {
      AddEntry(GetEntryFromChange(*it), model_);
    }
    return SyncError();
  }

 private:
  EntryMap* model_;
};

ArticleEntry CreateEntry(std::string entry_id,
                         std::string page_url1,
                         std::string page_url2,
                         std::string page_url3) {
  ArticleEntry entry;
  entry.set_entry_id(entry_id);
  if (!page_url1.empty()) {
    ArticleEntryPage* page = entry.add_pages();
    page->set_url(page_url1);
  }
  if (!page_url2.empty()) {
    ArticleEntryPage* page = entry.add_pages();
    page->set_url(page_url2);
  }
  if (!page_url3.empty()) {
    ArticleEntryPage* page = entry.add_pages();
    page->set_url(page_url3);
  }
  return entry;
}

ArticleEntry GetSampleEntry(int id) {
  static ArticleEntry entries[] = {
      CreateEntry("entry0", "example.com/1", "example.com/2", "example.com/3"),
      CreateEntry("entry1", "example.com/1", "", ""),
      CreateEntry("entry2", "example.com/p1", "example.com/p2", ""),
      CreateEntry("entry3", "example.com/something/all", "", ""),
      CreateEntry("entry4", "example.com/somethingelse/1", "", ""),
      CreateEntry("entry5", "rock.example.com/p1", "rock.example.com/p2", ""),
      CreateEntry("entry7", "example.com/entry7/1", "example.com/entry7/2", ""),
      CreateEntry("entry8", "example.com/entry8/1", "", ""),
      CreateEntry("entry9", "example.com/entry9/all", "", ""), };
  EXPECT_LT(id, 9);
  return entries[id % 9];
}

}  // namespace

class DomDistillerStoreTest : public testing::Test {
 public:
  virtual void SetUp() {
    base::FilePath db_dir_ = base::FilePath(FILE_PATH_LITERAL("/fake/path"));
    db_model_.clear();
    sync_model_.clear();
    store_model_.clear();
    next_sync_id_ = 1;
  }

  virtual void TearDown() {
    store_.reset();
    fake_db_ = NULL;
    fake_sync_processor_ = NULL;
  }

  // Creates a simple DomDistillerStore initialized with |store_model_| and
  // with a FakeDB backed by |db_model_|.
  void CreateStore() {
    fake_db_ = new FakeDB(&db_model_);
    store_.reset(new DomDistillerStore(
        scoped_ptr<DomDistillerDatabaseInterface>(fake_db_),
        EntryMapToList(store_model_),
        db_dir_));
  }

  void StartSyncing() {
    fake_sync_processor_ = new FakeSyncChangeProcessor(&sync_model_);

    store_->MergeDataAndStartSyncing(
        kDomDistillerModelType,
        SyncDataFromEntryMap(sync_model_),
        make_scoped_ptr<SyncChangeProcessor>(fake_sync_processor_),
        scoped_ptr<SyncErrorFactory>(new FakeSyncErrorFactory()));
  }

 protected:
  SyncData CreateSyncData(const ArticleEntry& entry) {
    EntitySpecifics specifics = SpecificsFromEntry(entry);
    return SyncData::CreateRemoteData(
        next_sync_id_++, specifics, Time::UnixEpoch());
  }

  SyncDataList SyncDataFromEntryMap(const EntryMap& model) {
    SyncDataList data;
    for (EntryMap::const_iterator it = model.begin(); it != model.end(); ++it) {
      data.push_back(CreateSyncData(it->second));
    }
    return data;
  }

  EntryMap db_model_;
  EntryMap sync_model_;
  EntryMap store_model_;

  scoped_ptr<DomDistillerStore> store_;

  // Both owned by |store_|.
  FakeDB* fake_db_;
  FakeSyncChangeProcessor* fake_sync_processor_;

  int64 next_sync_id_;

  base::FilePath db_dir_;
};

AssertionResult AreEntriesEqual(const EntryVector& entries,
                                EntryMap expected_entries) {
  if (entries.size() != expected_entries.size())
    return AssertionFailure() << "Expected " << expected_entries.size()
                              << " entries but found " << entries.size();

  for (EntryVector::const_iterator it = entries.begin(); it != entries.end();
       ++it) {
    EntryMap::iterator expected_it = expected_entries.find(it->entry_id());
    if (expected_it == expected_entries.end()) {
      return AssertionFailure() << "Found unexpected entry with id <"
                                << it->entry_id() << ">";
    }
    if (!AreEntriesEqual(expected_it->second, *it)) {
      return AssertionFailure() << "Mismatched entry with id <"
                                << it->entry_id() << ">";
    }
    expected_entries.erase(expected_it);
  }
  return AssertionSuccess();
}

AssertionResult AreEntryMapsEqual(const EntryMap& left, const EntryMap& right) {
  EntryVector entries;
  for (EntryMap::const_iterator it = left.begin(); it != left.end(); ++it) {
    entries.push_back(it->second);
  }
  return AreEntriesEqual(entries, right);
}

TEST_F(DomDistillerStoreTest, TestDatabaseLoad) {
  AddEntry(GetSampleEntry(0), &db_model_);
  AddEntry(GetSampleEntry(1), &db_model_);
  AddEntry(GetSampleEntry(2), &db_model_);

  CreateStore();

  fake_db_->InitCallback(true);
  EXPECT_EQ(fake_db_->GetDirectory(), db_dir_);

  fake_db_->LoadCallback(true);
  EXPECT_TRUE(AreEntriesEqual(store_->GetEntries(), db_model_));
}

TEST_F(DomDistillerStoreTest, TestDatabaseLoadMerge) {
  AddEntry(GetSampleEntry(0), &db_model_);
  AddEntry(GetSampleEntry(1), &db_model_);
  AddEntry(GetSampleEntry(2), &db_model_);

  AddEntry(GetSampleEntry(2), &store_model_);
  AddEntry(GetSampleEntry(3), &store_model_);
  AddEntry(GetSampleEntry(4), &store_model_);

  EntryMap expected_model(db_model_);
  AddEntry(GetSampleEntry(3), &expected_model);
  AddEntry(GetSampleEntry(4), &expected_model);

  CreateStore();
  fake_db_->InitCallback(true);
  fake_db_->LoadCallback(true);

  EXPECT_TRUE(AreEntriesEqual(store_->GetEntries(), expected_model));
  EXPECT_TRUE(AreEntryMapsEqual(db_model_, expected_model));
}

TEST_F(DomDistillerStoreTest, TestAddEntry) {
  CreateStore();
  fake_db_->InitCallback(true);
  fake_db_->LoadCallback(true);

  EXPECT_TRUE(store_->GetEntries().empty());
  EXPECT_TRUE(db_model_.empty());

  store_->AddEntry(GetSampleEntry(0));

  EntryMap expected_model;
  AddEntry(GetSampleEntry(0), &expected_model);

  EXPECT_TRUE(AreEntriesEqual(store_->GetEntries(), expected_model));
  EXPECT_TRUE(AreEntryMapsEqual(db_model_, expected_model));
}

TEST_F(DomDistillerStoreTest, TestSyncMergeWithEmptyDatabase) {
  AddEntry(GetSampleEntry(0), &sync_model_);
  AddEntry(GetSampleEntry(1), &sync_model_);
  AddEntry(GetSampleEntry(2), &sync_model_);

  CreateStore();
  fake_db_->InitCallback(true);
  fake_db_->LoadCallback(true);

  StartSyncing();

  EXPECT_TRUE(AreEntriesEqual(store_->GetEntries(), sync_model_));
  EXPECT_TRUE(AreEntryMapsEqual(db_model_, sync_model_));
}

TEST_F(DomDistillerStoreTest, TestSyncMergeAfterDatabaseLoad) {
  AddEntry(GetSampleEntry(0), &db_model_);
  AddEntry(GetSampleEntry(1), &db_model_);
  AddEntry(GetSampleEntry(2), &db_model_);

  AddEntry(GetSampleEntry(2), &sync_model_);
  AddEntry(GetSampleEntry(3), &sync_model_);
  AddEntry(GetSampleEntry(4), &sync_model_);

  EntryMap expected_model(db_model_);
  AddEntry(GetSampleEntry(3), &expected_model);
  AddEntry(GetSampleEntry(4), &expected_model);

  CreateStore();
  fake_db_->InitCallback(true);
  fake_db_->LoadCallback(true);

  EXPECT_TRUE(AreEntriesEqual(store_->GetEntries(), db_model_));

  StartSyncing();

  EXPECT_TRUE(AreEntriesEqual(store_->GetEntries(), expected_model));
  EXPECT_TRUE(AreEntryMapsEqual(db_model_, expected_model));
  EXPECT_TRUE(AreEntryMapsEqual(sync_model_, expected_model));
}

TEST_F(DomDistillerStoreTest, TestGetAllSyncData) {
  AddEntry(GetSampleEntry(0), &db_model_);
  AddEntry(GetSampleEntry(1), &db_model_);
  AddEntry(GetSampleEntry(2), &db_model_);

  AddEntry(GetSampleEntry(2), &sync_model_);
  AddEntry(GetSampleEntry(3), &sync_model_);
  AddEntry(GetSampleEntry(4), &sync_model_);

  EntryMap expected_model(db_model_);
  AddEntry(GetSampleEntry(3), &expected_model);
  AddEntry(GetSampleEntry(4), &expected_model);

  CreateStore();

  fake_db_->InitCallback(true);
  fake_db_->LoadCallback(true);

  StartSyncing();

  SyncDataList data = store_->GetAllSyncData(kDomDistillerModelType);
  EntryVector entries;
  for (SyncDataList::iterator it = data.begin(); it != data.end(); ++it) {
    entries.push_back(EntryFromSpecifics(it->GetSpecifics()));
  }
  EXPECT_TRUE(AreEntriesEqual(entries, expected_model));
}

TEST_F(DomDistillerStoreTest, TestProcessSyncChanges) {
  AddEntry(GetSampleEntry(0), &db_model_);
  AddEntry(GetSampleEntry(1), &db_model_);
  sync_model_ = db_model_;

  EntryMap expected_model(db_model_);
  AddEntry(GetSampleEntry(2), &expected_model);
  AddEntry(GetSampleEntry(3), &expected_model);

  CreateStore();

  fake_db_->InitCallback(true);
  fake_db_->LoadCallback(true);

  StartSyncing();

  SyncChangeList changes;
  changes.push_back(SyncChange(
      FROM_HERE, SyncChange::ACTION_ADD, CreateSyncData(GetSampleEntry(2))));
  changes.push_back(SyncChange(
      FROM_HERE, SyncChange::ACTION_ADD, CreateSyncData(GetSampleEntry(3))));

  store_->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_TRUE(AreEntriesEqual(store_->GetEntries(), expected_model));
  EXPECT_TRUE(AreEntryMapsEqual(db_model_, expected_model));
}

TEST_F(DomDistillerStoreTest, TestSyncMergeWithSecondDomDistillerStore) {
  AddEntry(GetSampleEntry(0), &db_model_);
  AddEntry(GetSampleEntry(1), &db_model_);
  AddEntry(GetSampleEntry(2), &db_model_);

  EntryMap other_db_model;
  AddEntry(GetSampleEntry(2), &other_db_model);
  AddEntry(GetSampleEntry(3), &other_db_model);
  AddEntry(GetSampleEntry(4), &other_db_model);

  EntryMap expected_model(db_model_);
  AddEntry(GetSampleEntry(3), &expected_model);
  AddEntry(GetSampleEntry(4), &expected_model);

  CreateStore();

  fake_db_->InitCallback(true);
  fake_db_->LoadCallback(true);

  FakeDB* other_fake_db = new FakeDB(&other_db_model);
  scoped_ptr<DomDistillerStore> owned_other_store(new DomDistillerStore(
      scoped_ptr<DomDistillerDatabaseInterface>(other_fake_db),
      std::vector<ArticleEntry>(),
      base::FilePath(FILE_PATH_LITERAL("/fake/other/path"))));
  DomDistillerStore* other_store = owned_other_store.get();
  other_fake_db->InitCallback(true);
  other_fake_db->LoadCallback(true);

  EXPECT_FALSE(AreEntriesEqual(store_->GetEntries(), expected_model));
  EXPECT_FALSE(AreEntriesEqual(other_store->GetEntries(), expected_model));
  ASSERT_TRUE(AreEntriesEqual(other_store->GetEntries(), other_db_model));

  FakeSyncErrorFactory* other_error_factory = new FakeSyncErrorFactory();
  store_->MergeDataAndStartSyncing(
      kDomDistillerModelType,
      SyncDataFromEntryMap(other_db_model),
      owned_other_store.PassAs<SyncChangeProcessor>(),
      make_scoped_ptr<SyncErrorFactory>(other_error_factory));

  EXPECT_TRUE(AreEntriesEqual(store_->GetEntries(), expected_model));
  EXPECT_TRUE(AreEntriesEqual(other_store->GetEntries(), expected_model));
}

}  // namespace dom_distiller
