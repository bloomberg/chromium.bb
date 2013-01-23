// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using chrome::spellcheck_common::WordList;

namespace {

// Get all sync data for the custom dictionary without limiting to maximum
// number of syncable words.
syncer::SyncDataList GetAllSyncDataNoLimit(
    const SpellcheckCustomDictionary* dictionary) {
  syncer::SyncDataList data;
  std::string word;
  for (WordList::const_iterator it = dictionary->GetWords().begin();
       it != dictionary->GetWords().end();
       ++it) {
    word = *it;
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    data.push_back(syncer::SyncData::CreateLocalData(word, word, specifics));
  }
  return data;
}

}  // namespace

static ProfileKeyedService* BuildSpellcheckService(Profile* profile) {
  return new SpellcheckService(profile);
}

class SpellcheckCustomDictionaryTest : public testing::Test {
 protected:
  SpellcheckCustomDictionaryTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        profile_(new TestingProfile) {
  }

  void SetUp() OVERRIDE {
    // Use SetTestingFactoryAndUse to force creation and initialization.
    SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile_.get(), &BuildSpellcheckService);
  }

  void TearDown() OVERRIDE {
    MessageLoop::current()->RunUntilIdle();
  }

  // A wrapper around SpellcheckCustomDictionary::LoadDictionaryFile private
  // function to avoid a large number of FRIEND_TEST declarations in
  // SpellcheckCustomDictionary.
  chrome::spellcheck_common::WordList LoadDictionaryFile(const FilePath& path) {
    return SpellcheckCustomDictionary::LoadDictionaryFile(path);
  }

  // A wrapper around SpellcheckCustomDictionary::UpdateDictionaryFile private
  // function to avoid a large number of FRIEND_TEST declarations in
  // SpellcheckCustomDictionary.
  void UpdateDictionaryFile(
      const SpellcheckCustomDictionary::Change& dictionary_change,
      const FilePath& path) {
    SpellcheckCustomDictionary::UpdateDictionaryFile(dictionary_change, path);
  }

  // A wrapper around SpellcheckCustomDictionary::OnLoaded private method to
  // avoid a large number of FRIEND_TEST declarations in
  // SpellcheckCustomDictionary.
  void OnLoaded(
      SpellcheckCustomDictionary& dictionary,
      const chrome::spellcheck_common::WordList& custom_words) {
    dictionary.OnLoaded(custom_words);
  }

  // A wrapper around SpellcheckCustomDictionary::Apply private method to avoid
  // a large number of FRIEND_TEST declarations in SpellcheckCustomDictionary.
  void Apply(
      SpellcheckCustomDictionary& dictionary,
      const SpellcheckCustomDictionary::Change& change) {
    return dictionary.Apply(change);
  }

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  scoped_ptr<TestingProfile> profile_;
};

// A wrapper around SpellcheckCustomDictionary that does not own the wrapped
// object. An instance of this class can be inside of a scoped pointer safely
// while the dictionary is managed by another scoped pointer.
class SyncChangeProcessorDelegate : public syncer::SyncChangeProcessor {
 public:
  explicit SyncChangeProcessorDelegate(SpellcheckCustomDictionary* dictionary)
      : dictionary_(dictionary) {}
  virtual ~SyncChangeProcessorDelegate() {}

  // Overridden from syncer::SyncChangeProcessor:
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE {
    return dictionary_->ProcessSyncChanges(from_here, change_list);
  }

 private:
  SpellcheckCustomDictionary* dictionary_;
  DISALLOW_COPY_AND_ASSIGN(SyncChangeProcessorDelegate);
};

// An implementation of SyncErrorFactory that does not upload the error message
// and updates an outside error counter. This lets us know the number of error
// messages in an instance of this class after that instance is deleted.
class SyncErrorFactoryStub : public syncer::SyncErrorFactory {
 public:
  explicit SyncErrorFactoryStub(int* error_counter)
      : error_counter_(error_counter) {}
  virtual ~SyncErrorFactoryStub() {}

  // Overridden from syncer::SyncErrorFactory:
  virtual syncer::SyncError CreateAndUploadError(
      const tracked_objects::Location& location,
      const std::string& message) OVERRIDE {
    (*error_counter_)++;
    return syncer::SyncError(location, message, syncer::DICTIONARY);
  }

 private:
  int* error_counter_;
  DISALLOW_COPY_AND_ASSIGN(SyncErrorFactoryStub);
};

// Counts the number of notifications for dictionary load and change.
class DictionaryObserverCounter : public SpellcheckCustomDictionary::Observer {
 public:
  DictionaryObserverCounter() : loads_(0), changes_(0) {}
  virtual ~DictionaryObserverCounter() {}

  int loads() const { return loads_; }
  int changes() const { return changes_; }

  // Overridden from SpellcheckCustomDictionary::Observer:
  virtual void OnCustomDictionaryLoaded() OVERRIDE { loads_++; }
  virtual void OnCustomDictionaryChanged(
      const SpellcheckCustomDictionary::Change& change) OVERRIDE { changes_++; }

 private:
  int loads_;
  int changes_;
  DISALLOW_COPY_AND_ASSIGN(DictionaryObserverCounter);
};

TEST_F(SpellcheckCustomDictionaryTest, SaveAndLoad) {
  FilePath path = profile_->GetPath().Append(chrome::kCustomDictionaryFileName);
  WordList loaded_custom_words = LoadDictionaryFile(path);

  // The custom word list should be empty now.
  WordList expected;
  EXPECT_EQ(expected, loaded_custom_words);

  SpellcheckCustomDictionary::Change change;
  change.AddWord("bar");
  change.AddWord("foo");

  UpdateDictionaryFile(change, path);
  expected.push_back("bar");
  expected.push_back("foo");

  // The custom word list should include written words.
  loaded_custom_words = LoadDictionaryFile(path);
  EXPECT_EQ(expected, loaded_custom_words);

  change = SpellcheckCustomDictionary::Change();
  change.RemoveWord("bar");
  change.RemoveWord("foo");
  UpdateDictionaryFile(change, path);
  loaded_custom_words = LoadDictionaryFile(path);
  expected.clear();
  EXPECT_EQ(expected, loaded_custom_words);

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest, MultiProfile) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 =
      static_cast<SpellcheckService*>(
          SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile2, &BuildSpellcheckService));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  WordList expected1;
  WordList expected2;

  custom_dictionary->AddWord("foo");
  custom_dictionary->AddWord("bar");
  expected1.push_back("foo");
  expected1.push_back("bar");

  custom_dictionary2->AddWord("hoge");
  custom_dictionary2->AddWord("fuga");
  expected2.push_back("hoge");
  expected2.push_back("fuga");

  WordList actual1 = custom_dictionary->GetWords();
  std::sort(actual1.begin(), actual1.end());
  std::sort(expected1.begin(), expected1.end());
  EXPECT_EQ(actual1, expected1);

  WordList actual2 = custom_dictionary2->GetWords();
  std::sort(actual2.begin(), actual2.end());
  std::sort(expected2.begin(), expected2.end());
  EXPECT_EQ(actual2, expected2);

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

// Legacy empty dictionary should be converted to new format empty dictionary.
TEST_F(SpellcheckCustomDictionaryTest, LegacyEmptyDictionaryShouldBeConverted) {
  FilePath path = profile_->GetPath().Append(chrome::kCustomDictionaryFileName);

  std::string content;
  file_util::WriteFile(path, content.c_str(), content.length());
  WordList loaded_custom_words = LoadDictionaryFile(path);
  EXPECT_TRUE(loaded_custom_words.empty());

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

// Legacy dictionary with two words should be converted to new format dictionary
// with two words.
TEST_F(SpellcheckCustomDictionaryTest,
       LegacyDictionaryWithTwoWordsShouldBeConverted) {
  FilePath path = profile_->GetPath().Append(chrome::kCustomDictionaryFileName);

  std::string content = "foo\nbar\nfoo\n";
  file_util::WriteFile(path, content.c_str(), content.length());
  WordList loaded_custom_words = LoadDictionaryFile(path);
  WordList expected;
  expected.push_back("bar");
  expected.push_back("foo");
  EXPECT_EQ(expected, loaded_custom_words);

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

// Words with spaces are illegal and should be removed.
TEST_F(SpellcheckCustomDictionaryTest,
       IllegalWordsShouldBeRemovedFromDictionary) {
  FilePath path = profile_->GetPath().Append(chrome::kCustomDictionaryFileName);

  std::string content = "foo\nfoo bar\nbar\nfoo bar";
  file_util::WriteFile(path, content.c_str(), content.length());
  WordList loaded_custom_words = LoadDictionaryFile(path);
  WordList expected;
  expected.push_back("bar");
  expected.push_back("foo");
  EXPECT_EQ(expected, loaded_custom_words);

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

// Write to dictionary should backup previous version and write the word to the
// end of the dictionary. If the dictionary file is corrupted on disk, the
// previous version should be reloaded.
TEST_F(SpellcheckCustomDictionaryTest, CorruptedWriteShouldBeRecovered) {
  FilePath path = profile_->GetPath().Append(chrome::kCustomDictionaryFileName);

  std::string content = "foo\nbar";
  file_util::WriteFile(path, content.c_str(), content.length());
  WordList loaded_custom_words = LoadDictionaryFile(path);
  WordList expected;
  expected.push_back("bar");
  expected.push_back("foo");
  EXPECT_EQ(expected, loaded_custom_words);

  SpellcheckCustomDictionary::Change change;
  change.AddWord("baz");
  UpdateDictionaryFile(change, path);
  content.clear();
  file_util::ReadFileToString(path, &content);
  content.append("corruption");
  file_util::WriteFile(path, content.c_str(), content.length());
  loaded_custom_words = LoadDictionaryFile(path);
  EXPECT_EQ(expected, loaded_custom_words);

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest,
       GetAllSyncDataAccuratelyReflectsDictionaryState) {
  SpellcheckCustomDictionary* dictionary =
      SpellcheckServiceFactory::GetForProfile(
          profile_.get())->GetCustomDictionary();

  syncer::SyncDataList data = dictionary->GetAllSyncData(syncer::DICTIONARY);
  EXPECT_TRUE(data.empty());

  EXPECT_TRUE(dictionary->AddWord("foo"));
  EXPECT_TRUE(dictionary->AddWord("bar"));

  data = dictionary->GetAllSyncData(syncer::DICTIONARY);
  EXPECT_EQ(2UL, data.size());
  std::vector<std::string> words;
  words.push_back("foo");
  words.push_back("bar");
  for (size_t i = 0; i < data.size(); i++) {
    EXPECT_TRUE(data[i].GetSpecifics().has_dictionary());
    EXPECT_EQ(syncer::DICTIONARY, data[i].GetDataType());
    EXPECT_EQ(words[i], data[i].GetTag());
    EXPECT_EQ(words[i], data[i].GetSpecifics().dictionary().word());
  }

  EXPECT_TRUE(dictionary->RemoveWord("foo"));
  EXPECT_TRUE(dictionary->RemoveWord("bar"));

  data = dictionary->GetAllSyncData(syncer::DICTIONARY);
  EXPECT_TRUE(data.empty());

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest, GetAllSyncDataHasLimit) {
  SpellcheckCustomDictionary* dictionary =
      SpellcheckServiceFactory::GetForProfile(
          profile_.get())->GetCustomDictionary();

  SpellcheckCustomDictionary::Change change;
  for (size_t i = 0;
       i < chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS - 1;
       i++) {
    change.AddWord("foo" + base::Uint64ToString(i));
  }
  Apply(*dictionary, change);
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS - 1,
            dictionary->GetWords().size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS - 1,
            dictionary->GetAllSyncData(syncer::DICTIONARY).size());

  dictionary->AddWord("baz");
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            dictionary->GetWords().size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            dictionary->GetAllSyncData(syncer::DICTIONARY).size());

  dictionary->AddWord("bar");
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS + 1,
            dictionary->GetWords().size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            dictionary->GetAllSyncData(syncer::DICTIONARY).size());

  dictionary->AddWord("snafoo");
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS + 2,
            dictionary->GetWords().size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            dictionary->GetAllSyncData(syncer::DICTIONARY).size());

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest, ProcessSyncChanges) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  SpellcheckCustomDictionary* dictionary =
      spellcheck_service->GetCustomDictionary();

  dictionary->AddWord("foo");
  dictionary->AddWord("bar");

  syncer::SyncChangeList changes;
  {
    // Add existing word.
    std::string word = "foo";
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    changes.push_back(syncer::SyncChange(
        FROM_HERE,
        syncer::SyncChange::ACTION_ADD,
        syncer::SyncData::CreateLocalData(word, word, specifics)));
  }
  {
    // Add invalid word.
    std::string word = "foo bar";
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    changes.push_back(syncer::SyncChange(
        FROM_HERE,
        syncer::SyncChange::ACTION_ADD,
        syncer::SyncData::CreateLocalData(word, word, specifics)));
  }
  {
    // Add valid word.
    std::string word = "baz";
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    changes.push_back(syncer::SyncChange(
        FROM_HERE,
        syncer::SyncChange::ACTION_ADD,
        syncer::SyncData::CreateLocalData(word, word, specifics)));
  }
  {
    // Remove missing word.
    std::string word = "snafoo";
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    changes.push_back(syncer::SyncChange(
        FROM_HERE,
        syncer::SyncChange::ACTION_DELETE,
        syncer::SyncData::CreateLocalData(word, word, specifics)));
  }
  {
    // Remove existing word.
    std::string word = "bar";
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    changes.push_back(syncer::SyncChange(
        FROM_HERE,
        syncer::SyncChange::ACTION_DELETE,
        syncer::SyncData::CreateLocalData(word, word, specifics)));
  }

  EXPECT_FALSE(dictionary->ProcessSyncChanges(FROM_HERE, changes).IsSet());

  const chrome::spellcheck_common::WordList& words = dictionary->GetWords();
  EXPECT_EQ(2UL, words.size());
  EXPECT_EQ(words.end(), std::find(words.begin(), words.end(), "bar"));
  EXPECT_NE(words.end(), std::find(words.begin(), words.end(), "foo"));
  EXPECT_NE(words.end(), std::find(words.begin(), words.end(), "baz"));

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest, MergeDataAndStartSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 =
      static_cast<SpellcheckService*>(
          SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile2, &BuildSpellcheckService));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  SpellcheckCustomDictionary::Change change;
  for (size_t i = 0;
       i < chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS / 2;
       ++i) {
    change.AddWord("foo" + base::Uint64ToString(i));
  }
  Apply(*custom_dictionary, change);

  SpellcheckCustomDictionary::Change change2;
  for (size_t i = 0;
       i < chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS / 2;
       ++i) {
    change2.AddWord("bar" + base::Uint64ToString(i));
  }
  Apply(*custom_dictionary2, change2);

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary->MergeDataAndStartSyncing(
      syncer::DICTIONARY,
      custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
      scoped_ptr<syncer::SyncChangeProcessor>(
          new SyncChangeProcessorDelegate(custom_dictionary2)),
      scoped_ptr<syncer::SyncErrorFactory>(
          new SyncErrorFactoryStub(&error_counter))).error().IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  WordList words = custom_dictionary->GetWords();
  WordList words2 = custom_dictionary2->GetWords();
  EXPECT_EQ(words.size(), words2.size());

  std::sort(words.begin(), words.end());
  std::sort(words2.begin(), words2.end());
  EXPECT_EQ(words, words2);

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryTooBigBeforeSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 =
      static_cast<SpellcheckService*>(
          SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile2, &BuildSpellcheckService));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  SpellcheckCustomDictionary::Change change;
  for (size_t i = 0;
       i < chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS + 1;
       ++i) {
    change.AddWord("foo" + base::Uint64ToString(i));
  }
  Apply(*custom_dictionary, change);

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary->MergeDataAndStartSyncing(
      syncer::DICTIONARY,
      custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
      scoped_ptr<syncer::SyncChangeProcessor>(
          new SyncChangeProcessorDelegate(custom_dictionary2)),
      scoped_ptr<syncer::SyncErrorFactory>(
          new SyncErrorFactoryStub(&error_counter))).error().IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_FALSE(custom_dictionary->IsSyncing());

  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary2->GetWords().size());

  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary->GetAllSyncData(syncer::DICTIONARY).size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary2->GetAllSyncData(syncer::DICTIONARY).size());

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryTooBigAndServerFull) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 =
      static_cast<SpellcheckService*>(
          SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile2, &BuildSpellcheckService));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  SpellcheckCustomDictionary::Change change;
  SpellcheckCustomDictionary::Change change2;
  for (size_t i = 0;
       i < chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS;
       ++i) {
    change.AddWord("foo" + base::Uint64ToString(i));
    change2.AddWord("bar" + base::Uint64ToString(i));
  }
  change.AddWord("foo");
  Apply(*custom_dictionary, change);
  Apply(*custom_dictionary2, change2);

  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary2->GetWords().size());

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary->MergeDataAndStartSyncing(
      syncer::DICTIONARY,
      custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
      scoped_ptr<syncer::SyncChangeProcessor>(
          new SyncChangeProcessorDelegate(custom_dictionary2)),
      scoped_ptr<syncer::SyncErrorFactory>(
          new SyncErrorFactoryStub(&error_counter))).error().IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_FALSE(custom_dictionary->IsSyncing());

  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS * 2 + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary2->GetWords().size());

  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary->GetAllSyncData(syncer::DICTIONARY).size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary2->GetAllSyncData(syncer::DICTIONARY).size());

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest, ServerTooBig) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 =
      static_cast<SpellcheckService*>(
          SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile2, &BuildSpellcheckService));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  SpellcheckCustomDictionary::Change change;
  SpellcheckCustomDictionary::Change change2;
  for (size_t i = 0;
       i < chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS + 1;
       ++i) {
    change.AddWord("foo" + base::Uint64ToString(i));
    change2.AddWord("bar" + base::Uint64ToString(i));
  }
  Apply(*custom_dictionary, change);
  Apply(*custom_dictionary2, change2);

  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS + 1,
            custom_dictionary2->GetWords().size());

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary->MergeDataAndStartSyncing(
      syncer::DICTIONARY,
      GetAllSyncDataNoLimit(custom_dictionary2),
      scoped_ptr<syncer::SyncChangeProcessor>(
          new SyncChangeProcessorDelegate(custom_dictionary2)),
      scoped_ptr<syncer::SyncErrorFactory>(
          new SyncErrorFactoryStub(&error_counter))).error().IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_FALSE(custom_dictionary->IsSyncing());

  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS * 2 + 2,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS + 1,
            custom_dictionary2->GetWords().size());

  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary->GetAllSyncData(syncer::DICTIONARY).size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary2->GetAllSyncData(syncer::DICTIONARY).size());

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryTooBigToStartSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 =
      static_cast<SpellcheckService*>(
          SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile2, &BuildSpellcheckService));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  SpellcheckCustomDictionary::Change change;
  for (size_t i = 0;
       i < chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS - 1;
       ++i) {
    change.AddWord("foo" + base::Uint64ToString(i));
  }
  Apply(*custom_dictionary, change);

  custom_dictionary2->AddWord("bar");
  custom_dictionary2->AddWord("baz");

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary->MergeDataAndStartSyncing(
      syncer::DICTIONARY,
      custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
      scoped_ptr<syncer::SyncChangeProcessor>(
          new SyncChangeProcessorDelegate(custom_dictionary2)),
      scoped_ptr<syncer::SyncErrorFactory>(
          new SyncErrorFactoryStub(&error_counter))).error().IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_FALSE(custom_dictionary->IsSyncing());

  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary2->GetWords().size());

  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary->GetAllSyncData(syncer::DICTIONARY).size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary2->GetAllSyncData(syncer::DICTIONARY).size());

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryTooBigToContiueSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 =
      static_cast<SpellcheckService*>(
          SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile2, &BuildSpellcheckService));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  SpellcheckCustomDictionary::Change change;
  for (size_t i = 0;
       i < chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS - 1;
       ++i) {
    change.AddWord("foo" + base::Uint64ToString(i));
  }
  Apply(*custom_dictionary, change);

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary->MergeDataAndStartSyncing(
      syncer::DICTIONARY,
      custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
      scoped_ptr<syncer::SyncChangeProcessor>(
          new SyncChangeProcessorDelegate(custom_dictionary2)),
      scoped_ptr<syncer::SyncErrorFactory>(
          new SyncErrorFactoryStub(&error_counter))).error().IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  custom_dictionary->AddWord("bar");
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  custom_dictionary->AddWord("baz");
  EXPECT_EQ(0, error_counter);
  EXPECT_FALSE(custom_dictionary->IsSyncing());

  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary2->GetWords().size());

  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary->GetAllSyncData(syncer::DICTIONARY).size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary2->GetAllSyncData(syncer::DICTIONARY).size());

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest, LoadAfterSyncStart) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 =
      static_cast<SpellcheckService*>(
          SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile2, &BuildSpellcheckService));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  custom_dictionary->AddWord("foo");

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary->MergeDataAndStartSyncing(
      syncer::DICTIONARY,
      custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
      scoped_ptr<syncer::SyncChangeProcessor>(
          new SyncChangeProcessorDelegate(custom_dictionary2)),
      scoped_ptr<syncer::SyncErrorFactory>(
          new SyncErrorFactoryStub(&error_counter))).error().IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  WordList custom_words;
  custom_words.push_back("bar");
  OnLoaded(*custom_dictionary, custom_words);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  EXPECT_EQ(2UL, custom_dictionary->GetWords().size());
  EXPECT_EQ(2UL, custom_dictionary2->GetWords().size());

  EXPECT_EQ(2UL, custom_dictionary->GetAllSyncData(syncer::DICTIONARY).size());
  EXPECT_EQ(2UL, custom_dictionary2->GetAllSyncData(syncer::DICTIONARY).size());

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest, LoadAfterSyncStartTooBigToSync) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 =
      static_cast<SpellcheckService*>(
          SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile2, &BuildSpellcheckService));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  custom_dictionary->AddWord("foo");

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary->MergeDataAndStartSyncing(
      syncer::DICTIONARY,
      custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
      scoped_ptr<syncer::SyncChangeProcessor>(
          new SyncChangeProcessorDelegate(custom_dictionary2)),
      scoped_ptr<syncer::SyncErrorFactory>(
          new SyncErrorFactoryStub(&error_counter))).error().IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  WordList custom_words;
  for (size_t i = 0;
       i < chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS;
       ++i) {
    custom_words.push_back("foo" + base::Uint64ToString(i));
  }
  OnLoaded(*custom_dictionary, custom_words);
  EXPECT_EQ(0, error_counter);
  EXPECT_FALSE(custom_dictionary->IsSyncing());

  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary2->GetWords().size());

  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary->GetAllSyncData(syncer::DICTIONARY).size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            custom_dictionary2->GetAllSyncData(syncer::DICTIONARY).size());

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest, LoadDuplicatesAfterSync) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 =
      static_cast<SpellcheckService*>(
          SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile2, &BuildSpellcheckService));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  WordList to_add;
  for (size_t i = 0;
       i < chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS / 2;
       ++i) {
    to_add.push_back("foo" + base::Uint64ToString(i));
  }
  Apply(*custom_dictionary, SpellcheckCustomDictionary::Change(to_add));

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary->MergeDataAndStartSyncing(
      syncer::DICTIONARY,
      custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
      scoped_ptr<syncer::SyncChangeProcessor>(
          new SyncChangeProcessorDelegate(custom_dictionary2)),
      scoped_ptr<syncer::SyncErrorFactory>(
          new SyncErrorFactoryStub(&error_counter))).error().IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  OnLoaded(*custom_dictionary, to_add);
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS / 2,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS / 2,
            custom_dictionary2->GetWords().size());

  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS / 2,
            custom_dictionary->GetAllSyncData(syncer::DICTIONARY).size());
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS / 2,
            custom_dictionary2->GetAllSyncData(syncer::DICTIONARY).size());

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryLoadNotification) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();

  DictionaryObserverCounter observer;
  custom_dictionary->AddObserver(&observer);

  WordList custom_words;
  custom_words.push_back("foo");
  custom_words.push_back("bar");
  OnLoaded(*custom_dictionary, custom_words);

  EXPECT_GE(observer.loads(), 1);
  EXPECT_LE(observer.loads(), 2);
  EXPECT_EQ(0, observer.changes());

  custom_dictionary->RemoveObserver(&observer);

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryAddWordNotification) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();

  OnLoaded(*custom_dictionary, WordList());

  DictionaryObserverCounter observer;
  custom_dictionary->AddObserver(&observer);

  EXPECT_TRUE(custom_dictionary->AddWord("foo"));
  EXPECT_TRUE(custom_dictionary->AddWord("bar"));
  EXPECT_FALSE(custom_dictionary->AddWord("bar"));

  EXPECT_EQ(2, observer.changes());

  custom_dictionary->RemoveObserver(&observer);

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryRemoveWordNotification) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();

  OnLoaded(*custom_dictionary, WordList());

  EXPECT_TRUE(custom_dictionary->AddWord("foo"));
  EXPECT_TRUE(custom_dictionary->AddWord("bar"));

  DictionaryObserverCounter observer;
  custom_dictionary->AddObserver(&observer);

  EXPECT_TRUE(custom_dictionary->RemoveWord("foo"));
  EXPECT_TRUE(custom_dictionary->RemoveWord("bar"));
  EXPECT_FALSE(custom_dictionary->RemoveWord("baz"));

  EXPECT_EQ(2, observer.changes());

  custom_dictionary->RemoveObserver(&observer);

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

TEST_F(SpellcheckCustomDictionaryTest, DictionarySyncNotification) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForProfile(profile_.get());
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 =
      static_cast<SpellcheckService*>(
          SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile2, &BuildSpellcheckService));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  OnLoaded(*custom_dictionary, WordList());
  OnLoaded(*custom_dictionary2, WordList());

  custom_dictionary->AddWord("foo");
  custom_dictionary->AddWord("bar");
  custom_dictionary2->AddWord("foo");
  custom_dictionary2->AddWord("baz");

  DictionaryObserverCounter observer;
  custom_dictionary->AddObserver(&observer);

  DictionaryObserverCounter observer2;
  custom_dictionary2->AddObserver(&observer2);

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary->MergeDataAndStartSyncing(
      syncer::DICTIONARY,
      custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
      scoped_ptr<syncer::SyncChangeProcessor>(
          new SyncChangeProcessorDelegate(custom_dictionary2)),
      scoped_ptr<syncer::SyncErrorFactory>(
          new SyncErrorFactoryStub(&error_counter))).error().IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  EXPECT_EQ(1, observer.changes());
  EXPECT_EQ(1, observer2.changes());

  custom_dictionary->RemoveObserver(&observer);
  custom_dictionary2->RemoveObserver(&observer2);

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}

// The server has maximum number of words and the client has maximum number of
// different words before association time. No new words should be pushed to the
// sync server upon association. The client should accept words from the sync
// server, however.
TEST_F(SpellcheckCustomDictionaryTest, DictionarySyncLimit) {
  TestingProfile server_profile;
  SpellcheckService* server_spellcheck_service =
      static_cast<SpellcheckService*>(
          SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &server_profile, &BuildSpellcheckService));

  // Here, |server_custom_dictionary| plays the role of the sync server.
  SpellcheckCustomDictionary* server_custom_dictionary =
      server_spellcheck_service->GetCustomDictionary();

  // Upload the maximum number of words to the sync server.
  {
    SpellcheckService* spellcheck_service =
        SpellcheckServiceFactory::GetForProfile(profile_.get());
    SpellcheckCustomDictionary* custom_dictionary =
        spellcheck_service->GetCustomDictionary();

    SpellcheckCustomDictionary::Change change;
    for (size_t i = 0;
         i < chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS;
         ++i) {
      change.AddWord("foo" + base::Uint64ToString(i));
    }
    Apply(*custom_dictionary, change);

    int error_counter = 0;
    EXPECT_FALSE(custom_dictionary->MergeDataAndStartSyncing(
        syncer::DICTIONARY,
        server_custom_dictionary->GetAllSyncData(syncer::DICTIONARY),
        scoped_ptr<syncer::SyncChangeProcessor>(
            new SyncChangeProcessorDelegate(server_custom_dictionary)),
        scoped_ptr<syncer::SyncErrorFactory>(
            new SyncErrorFactoryStub(&error_counter))).error().IsSet());
    EXPECT_EQ(0, error_counter);
    EXPECT_TRUE(custom_dictionary->IsSyncing());
    EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
              custom_dictionary->GetWords().size());
  }

  // The sync server now has the maximum number of words.
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            server_custom_dictionary->GetWords().size());

  // Associate the sync server with a client that also has the maximum number of
  // words, but all of these words are different from the ones on the sync
  // server.
  {
    TestingProfile client_profile;
    SpellcheckService* client_spellcheck_service =
        static_cast<SpellcheckService*>(
            SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
                &client_profile, &BuildSpellcheckService));

    // Here, |client_custom_dictionary| plays the role of the client.
    SpellcheckCustomDictionary* client_custom_dictionary =
        client_spellcheck_service->GetCustomDictionary();

    // Add the maximum number of words to the client. These words are all
    // different from those on the server.
    SpellcheckCustomDictionary::Change change;
    for (size_t i = 0;
         i < chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS;
         ++i) {
      change.AddWord("bar" + base::Uint64ToString(i));
    }
    Apply(*client_custom_dictionary, change);

    // Associate the server and the client.
    int error_counter = 0;
    EXPECT_FALSE(client_custom_dictionary->MergeDataAndStartSyncing(
        syncer::DICTIONARY,
        server_custom_dictionary->GetAllSyncData(syncer::DICTIONARY),
        scoped_ptr<syncer::SyncChangeProcessor>(
            new SyncChangeProcessorDelegate(server_custom_dictionary)),
        scoped_ptr<syncer::SyncErrorFactory>(
            new SyncErrorFactoryStub(&error_counter))).error().IsSet());
    EXPECT_EQ(0, error_counter);
    EXPECT_FALSE(client_custom_dictionary->IsSyncing());
    EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS * 2,
              client_custom_dictionary->GetWords().size());

    // Flush the loop now to prevent service init tasks from being run during
    // TearDown();
    MessageLoop::current()->RunUntilIdle();
  }

  // The sync server should not receive more words, because it has the maximum
  // number of words already.
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            server_custom_dictionary->GetWords().size());

  // Flush the loop now to prevent service init tasks from being run during
  // TearDown();
  MessageLoop::current()->RunUntilIdle();
}
