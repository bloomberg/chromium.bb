// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_util.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor_wrapper_for_test.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
// For version specific disabled tests below (http://crbug.com/230534).
#include "base/win/windows_version.h"
#endif

using base::HistogramBase;
using base::HistogramSamples;
using base::StatisticsRecorder;
using chrome::spellcheck_common::WordList;
using chrome::spellcheck_common::WordSet;

namespace {

// Get all sync data for the custom dictionary without limiting to maximum
// number of syncable words.
syncer::SyncDataList GetAllSyncDataNoLimit(
    const SpellcheckCustomDictionary* dictionary) {
  syncer::SyncDataList data;
  std::string word;
  const WordSet& words = dictionary->GetWords();
  for (WordSet::const_iterator it = words.begin(); it != words.end(); ++it) {
    word = *it;
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    data.push_back(syncer::SyncData::CreateLocalData(word, word, specifics));
  }
  return data;
}

}  // namespace

static KeyedService* BuildSpellcheckService(content::BrowserContext* profile) {
  return new SpellcheckService(static_cast<Profile*>(profile));
}

class SpellcheckCustomDictionaryTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    // Use SetTestingFactoryAndUse to force creation and initialization.
    SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        &profile_, &BuildSpellcheckService);

    StatisticsRecorder::Initialize();
  }

  // A wrapper around SpellcheckCustomDictionary::LoadDictionaryFile private
  // function to avoid a large number of FRIEND_TEST declarations in
  // SpellcheckCustomDictionary.
  chrome::spellcheck_common::WordList LoadDictionaryFile(
      const base::FilePath& path) {
    return SpellcheckCustomDictionary::LoadDictionaryFile(path);
  }

  // A wrapper around SpellcheckCustomDictionary::UpdateDictionaryFile private
  // function to avoid a large number of FRIEND_TEST declarations in
  // SpellcheckCustomDictionary.
  void UpdateDictionaryFile(
      const SpellcheckCustomDictionary::Change& dictionary_change,
      const base::FilePath& path) {
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

  content::TestBrowserThreadBundle thread_bundle_;

  TestingProfile profile_;
  net::TestURLFetcherFactory fetcher_factory_;
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
    return syncer::SyncError(location,
                             syncer::SyncError::DATATYPE_ERROR,
                             message,
                             syncer::DICTIONARY);
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
  base::FilePath path =
      profile_.GetPath().Append(chrome::kCustomDictionaryFileName);
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
}

TEST_F(SpellcheckCustomDictionaryTest, MultiProfile) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 =
      static_cast<SpellcheckService*>(
          SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile2, &BuildSpellcheckService));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  WordSet expected1;
  WordSet expected2;

  custom_dictionary->AddWord("foo");
  custom_dictionary->AddWord("bar");
  expected1.insert("foo");
  expected1.insert("bar");

  custom_dictionary2->AddWord("hoge");
  custom_dictionary2->AddWord("fuga");
  expected2.insert("hoge");
  expected2.insert("fuga");

  WordSet actual1 = custom_dictionary->GetWords();
  EXPECT_EQ(actual1, expected1);

  WordSet actual2 = custom_dictionary2->GetWords();
  EXPECT_EQ(actual2, expected2);
}

// Legacy empty dictionary should be converted to new format empty dictionary.
TEST_F(SpellcheckCustomDictionaryTest, LegacyEmptyDictionaryShouldBeConverted) {
  base::FilePath path =
      profile_.GetPath().Append(chrome::kCustomDictionaryFileName);

  std::string content;
  base::WriteFile(path, content.c_str(), content.length());
  WordList loaded_custom_words = LoadDictionaryFile(path);
  EXPECT_TRUE(loaded_custom_words.empty());
}

// Legacy dictionary with two words should be converted to new format dictionary
// with two words.
TEST_F(SpellcheckCustomDictionaryTest,
       LegacyDictionaryWithTwoWordsShouldBeConverted) {
  base::FilePath path =
      profile_.GetPath().Append(chrome::kCustomDictionaryFileName);

  std::string content = "foo\nbar\nfoo\n";
  base::WriteFile(path, content.c_str(), content.length());
  WordList loaded_custom_words = LoadDictionaryFile(path);
  WordList expected;
  expected.push_back("bar");
  expected.push_back("foo");
  EXPECT_EQ(expected, loaded_custom_words);
}

// Illegal words should be removed. Leading and trailing whitespace should be
// trimmed.
TEST_F(SpellcheckCustomDictionaryTest,
       IllegalWordsShouldBeRemovedFromDictionary) {
  base::FilePath path =
      profile_.GetPath().Append(chrome::kCustomDictionaryFileName);

  std::string content = "foo\n foo bar \n\n \nbar\n"
      "01234567890123456789012345678901234567890123456789"
      "01234567890123456789012345678901234567890123456789";
  base::WriteFile(path, content.c_str(), content.length());
  WordList loaded_custom_words = LoadDictionaryFile(path);
  WordList expected;
  expected.push_back("bar");
  expected.push_back("foo");
  expected.push_back("foo bar");
  EXPECT_EQ(expected, loaded_custom_words);
}

// Write to dictionary should backup previous version and write the word to the
// end of the dictionary. If the dictionary file is corrupted on disk, the
// previous version should be reloaded.
TEST_F(SpellcheckCustomDictionaryTest, CorruptedWriteShouldBeRecovered) {
  base::FilePath path =
      profile_.GetPath().Append(chrome::kCustomDictionaryFileName);

  std::string content = "foo\nbar";
  base::WriteFile(path, content.c_str(), content.length());
  WordList loaded_custom_words = LoadDictionaryFile(path);
  WordList expected;
  expected.push_back("bar");
  expected.push_back("foo");
  EXPECT_EQ(expected, loaded_custom_words);

  SpellcheckCustomDictionary::Change change;
  change.AddWord("baz");
  UpdateDictionaryFile(change, path);
  content.clear();
  base::ReadFileToString(path, &content);
  content.append("corruption");
  base::WriteFile(path, content.c_str(), content.length());
  loaded_custom_words = LoadDictionaryFile(path);
  EXPECT_EQ(expected, loaded_custom_words);
}

TEST_F(SpellcheckCustomDictionaryTest,
       GetAllSyncDataAccuratelyReflectsDictionaryState) {
  SpellcheckCustomDictionary* dictionary =
      SpellcheckServiceFactory::GetForContext(
          &profile_)->GetCustomDictionary();

  syncer::SyncDataList data = dictionary->GetAllSyncData(syncer::DICTIONARY);
  EXPECT_TRUE(data.empty());

  EXPECT_TRUE(dictionary->AddWord("bar"));
  EXPECT_TRUE(dictionary->AddWord("foo"));

  data = dictionary->GetAllSyncData(syncer::DICTIONARY);
  EXPECT_EQ(2UL, data.size());
  std::vector<std::string> words;
  words.push_back("bar");
  words.push_back("foo");
  for (size_t i = 0; i < data.size(); i++) {
    EXPECT_TRUE(data[i].GetSpecifics().has_dictionary());
    EXPECT_EQ(syncer::DICTIONARY, data[i].GetDataType());
    EXPECT_EQ(words[i], syncer::SyncDataLocal(data[i]).GetTag());
    EXPECT_EQ(words[i], data[i].GetSpecifics().dictionary().word());
  }

  EXPECT_TRUE(dictionary->RemoveWord("bar"));
  EXPECT_TRUE(dictionary->RemoveWord("foo"));

  data = dictionary->GetAllSyncData(syncer::DICTIONARY);
  EXPECT_TRUE(data.empty());
}

TEST_F(SpellcheckCustomDictionaryTest, GetAllSyncDataHasLimit) {
  SpellcheckCustomDictionary* dictionary =
      SpellcheckServiceFactory::GetForContext(
          &profile_)->GetCustomDictionary();

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
}

TEST_F(SpellcheckCustomDictionaryTest, ProcessSyncChanges) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
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
    // Add invalid word. This word is too long.
    std::string word = "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789";
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

  const WordSet& words = dictionary->GetWords();
  EXPECT_EQ(2UL, words.size());
  EXPECT_EQ(0UL, words.count("bar"));
  EXPECT_EQ(1UL, words.count("foo"));
  EXPECT_EQ(1UL, words.count("baz"));
}

TEST_F(SpellcheckCustomDictionaryTest, MergeDataAndStartSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
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
  EXPECT_FALSE(
      custom_dictionary->MergeDataAndStartSyncing(
                             syncer::DICTIONARY,
                             custom_dictionary2->GetAllSyncData(
                                 syncer::DICTIONARY),
                             scoped_ptr<syncer::SyncChangeProcessor>(
                                 new syncer::SyncChangeProcessorWrapperForTest(
                                     custom_dictionary2)),
                             scoped_ptr<syncer::SyncErrorFactory>(
                                 new SyncErrorFactoryStub(&error_counter)))
          .error()
          .IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  WordSet words = custom_dictionary->GetWords();
  WordSet words2 = custom_dictionary2->GetWords();
  EXPECT_EQ(words.size(), words2.size());
  EXPECT_EQ(words, words2);
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryTooBigBeforeSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
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
  EXPECT_FALSE(
      custom_dictionary->MergeDataAndStartSyncing(
                             syncer::DICTIONARY,
                             custom_dictionary2->GetAllSyncData(
                                 syncer::DICTIONARY),
                             scoped_ptr<syncer::SyncChangeProcessor>(
                                 new syncer::SyncChangeProcessorWrapperForTest(
                                     custom_dictionary2)),
                             scoped_ptr<syncer::SyncErrorFactory>(
                                 new SyncErrorFactoryStub(&error_counter)))
          .error()
          .IsSet());
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
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryTooBigAndServerFull) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
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
  EXPECT_FALSE(
      custom_dictionary->MergeDataAndStartSyncing(
                             syncer::DICTIONARY,
                             custom_dictionary2->GetAllSyncData(
                                 syncer::DICTIONARY),
                             scoped_ptr<syncer::SyncChangeProcessor>(
                                 new syncer::SyncChangeProcessorWrapperForTest(
                                     custom_dictionary2)),
                             scoped_ptr<syncer::SyncErrorFactory>(
                                 new SyncErrorFactoryStub(&error_counter)))
          .error()
          .IsSet());
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
}

TEST_F(SpellcheckCustomDictionaryTest, ServerTooBig) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
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
  EXPECT_FALSE(
      custom_dictionary->MergeDataAndStartSyncing(
                             syncer::DICTIONARY,
                             GetAllSyncDataNoLimit(custom_dictionary2),
                             scoped_ptr<syncer::SyncChangeProcessor>(
                                 new syncer::SyncChangeProcessorWrapperForTest(
                                     custom_dictionary2)),
                             scoped_ptr<syncer::SyncErrorFactory>(
                                 new SyncErrorFactoryStub(&error_counter)))
          .error()
          .IsSet());
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
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryTooBigToStartSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
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
  EXPECT_FALSE(
      custom_dictionary->MergeDataAndStartSyncing(
                             syncer::DICTIONARY,
                             custom_dictionary2->GetAllSyncData(
                                 syncer::DICTIONARY),
                             scoped_ptr<syncer::SyncChangeProcessor>(
                                 new syncer::SyncChangeProcessorWrapperForTest(
                                     custom_dictionary2)),
                             scoped_ptr<syncer::SyncErrorFactory>(
                                 new SyncErrorFactoryStub(&error_counter)))
          .error()
          .IsSet());
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
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryTooBigToContiueSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
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
  EXPECT_FALSE(
      custom_dictionary->MergeDataAndStartSyncing(
                             syncer::DICTIONARY,
                             custom_dictionary2->GetAllSyncData(
                                 syncer::DICTIONARY),
                             scoped_ptr<syncer::SyncChangeProcessor>(
                                 new syncer::SyncChangeProcessorWrapperForTest(
                                     custom_dictionary2)),
                             scoped_ptr<syncer::SyncErrorFactory>(
                                 new SyncErrorFactoryStub(&error_counter)))
          .error()
          .IsSet());
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
}

TEST_F(SpellcheckCustomDictionaryTest, LoadAfterSyncStart) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
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
  EXPECT_FALSE(
      custom_dictionary->MergeDataAndStartSyncing(
                             syncer::DICTIONARY,
                             custom_dictionary2->GetAllSyncData(
                                 syncer::DICTIONARY),
                             scoped_ptr<syncer::SyncChangeProcessor>(
                                 new syncer::SyncChangeProcessorWrapperForTest(
                                     custom_dictionary2)),
                             scoped_ptr<syncer::SyncErrorFactory>(
                                 new SyncErrorFactoryStub(&error_counter)))
          .error()
          .IsSet());
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
}

TEST_F(SpellcheckCustomDictionaryTest, LoadAfterSyncStartTooBigToSync) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
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
  EXPECT_FALSE(
      custom_dictionary->MergeDataAndStartSyncing(
                             syncer::DICTIONARY,
                             custom_dictionary2->GetAllSyncData(
                                 syncer::DICTIONARY),
                             scoped_ptr<syncer::SyncChangeProcessor>(
                                 new syncer::SyncChangeProcessorWrapperForTest(
                                     custom_dictionary2)),
                             scoped_ptr<syncer::SyncErrorFactory>(
                                 new SyncErrorFactoryStub(&error_counter)))
          .error()
          .IsSet());
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
}

TEST_F(SpellcheckCustomDictionaryTest, LoadDuplicatesAfterSync) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
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
  EXPECT_FALSE(
      custom_dictionary->MergeDataAndStartSyncing(
                             syncer::DICTIONARY,
                             custom_dictionary2->GetAllSyncData(
                                 syncer::DICTIONARY),
                             scoped_ptr<syncer::SyncChangeProcessor>(
                                 new syncer::SyncChangeProcessorWrapperForTest(
                                     custom_dictionary2)),
                             scoped_ptr<syncer::SyncErrorFactory>(
                                 new SyncErrorFactoryStub(&error_counter)))
          .error()
          .IsSet());
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
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryLoadNotification) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
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
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryAddWordNotification) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
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
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryRemoveWordNotification) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
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
}

TEST_F(SpellcheckCustomDictionaryTest, DictionarySyncNotification) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
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
  EXPECT_FALSE(
      custom_dictionary->MergeDataAndStartSyncing(
                             syncer::DICTIONARY,
                             custom_dictionary2->GetAllSyncData(
                                 syncer::DICTIONARY),
                             scoped_ptr<syncer::SyncChangeProcessor>(
                                 new syncer::SyncChangeProcessorWrapperForTest(
                                     custom_dictionary2)),
                             scoped_ptr<syncer::SyncErrorFactory>(
                                 new SyncErrorFactoryStub(&error_counter)))
          .error()
          .IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  EXPECT_EQ(1, observer.changes());
  EXPECT_EQ(1, observer2.changes());

  custom_dictionary->RemoveObserver(&observer);
  custom_dictionary2->RemoveObserver(&observer2);
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
        SpellcheckServiceFactory::GetForContext(&profile_);
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
    EXPECT_FALSE(
        custom_dictionary
            ->MergeDataAndStartSyncing(
                  syncer::DICTIONARY,
                  server_custom_dictionary->GetAllSyncData(syncer::DICTIONARY),
                  scoped_ptr<syncer::SyncChangeProcessor>(
                      new syncer::SyncChangeProcessorWrapperForTest(
                          server_custom_dictionary)),
                  scoped_ptr<syncer::SyncErrorFactory>(
                      new SyncErrorFactoryStub(&error_counter)))
            .error()
            .IsSet());
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
    EXPECT_FALSE(
        client_custom_dictionary
            ->MergeDataAndStartSyncing(
                  syncer::DICTIONARY,
                  server_custom_dictionary->GetAllSyncData(syncer::DICTIONARY),
                  scoped_ptr<syncer::SyncChangeProcessor>(
                      new syncer::SyncChangeProcessorWrapperForTest(
                          server_custom_dictionary)),
                  scoped_ptr<syncer::SyncErrorFactory>(
                      new SyncErrorFactoryStub(&error_counter)))
            .error()
            .IsSet());
    EXPECT_EQ(0, error_counter);
    EXPECT_FALSE(client_custom_dictionary->IsSyncing());
    EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS * 2,
              client_custom_dictionary->GetWords().size());
  }

  // The sync server should not receive more words, because it has the maximum
  // number of words already.
  EXPECT_EQ(chrome::spellcheck_common::MAX_SYNCABLE_DICTIONARY_WORDS,
            server_custom_dictionary->GetWords().size());
}

TEST_F(SpellcheckCustomDictionaryTest, RecordSizeStatsCorrectly) {
#if defined(OS_WIN)
// Failing consistently on Win7. See crbug.com/230534.
  if (base::win::GetVersion() >= base::win::VERSION_VISTA)
    return;
#endif
  // Record a baseline.
  SpellCheckHostMetrics::RecordCustomWordCountStats(123);

  // Determine if test failures are due the statistics recorder not being
  // available or because the histogram just isn't there: crbug.com/230534.
  EXPECT_TRUE(StatisticsRecorder::IsActive());

  HistogramBase* histogram =
      StatisticsRecorder::FindHistogram("SpellCheck.CustomWords");
  ASSERT_TRUE(histogram != NULL);
  scoped_ptr<HistogramSamples> baseline = histogram->SnapshotSamples();

  // Load the dictionary which should be empty.
  base::FilePath path =
      profile_.GetPath().Append(chrome::kCustomDictionaryFileName);
  WordList loaded_custom_words = LoadDictionaryFile(path);
  EXPECT_EQ(0u, loaded_custom_words.size());

  // We expect there to be an entry with 0.
  histogram =
      StatisticsRecorder::FindHistogram("SpellCheck.CustomWords");
  ASSERT_TRUE(histogram != NULL);
  scoped_ptr<HistogramSamples> samples = histogram->SnapshotSamples();

  samples->Subtract(*baseline);
  EXPECT_EQ(0,samples->sum());

  SpellcheckCustomDictionary::Change change;
  change.AddWord("bar");
  change.AddWord("foo");
  UpdateDictionaryFile(change, path);

  // Load the dictionary again and it should have 2 entries.
  loaded_custom_words = LoadDictionaryFile(path);
  EXPECT_EQ(2u, loaded_custom_words.size());

  histogram =
      StatisticsRecorder::FindHistogram("SpellCheck.CustomWords");
  ASSERT_TRUE(histogram != NULL);
  scoped_ptr<HistogramSamples> samples2 = histogram->SnapshotSamples();

  samples2->Subtract(*baseline);
  EXPECT_EQ(2,samples2->sum());
}

TEST_F(SpellcheckCustomDictionaryTest, HasWord) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  OnLoaded(*custom_dictionary, WordList());
  EXPECT_FALSE(custom_dictionary->HasWord("foo"));
  EXPECT_FALSE(custom_dictionary->HasWord("bar"));
  custom_dictionary->AddWord("foo");
  EXPECT_TRUE(custom_dictionary->HasWord("foo"));
  EXPECT_FALSE(custom_dictionary->HasWord("bar"));
}
