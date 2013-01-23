// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/dictionary_helper.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/dictionary_load_observer.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/spellcheck_common.h"
#include "content/public/test/test_utils.h"

class DictionarySyncIntegrationTestHelper {
 public:
  // Same as SpellcheckCustomDictionary::AddWord/RemoveWord, except does not
  // write to disk.
  static bool ApplyChange(
      SpellcheckCustomDictionary* dictionary,
      SpellcheckCustomDictionary::Change& change) {
    std::sort(dictionary->words_.begin(), dictionary->words_.end());
    int result = change.Sanitize(dictionary->GetWords());
    dictionary->Apply(change);
    dictionary->Notify(change);
    dictionary->Sync(change);
    return !result;
  }

  DISALLOW_COPY_AND_ASSIGN(DictionarySyncIntegrationTestHelper);
};


namespace dictionary_helper {
namespace {

bool DictionaryHasWord(
    const SpellcheckCustomDictionary* dictionary,
    const std::string& word) {
  return dictionary->GetWords().end() != std::find(
      dictionary->GetWords().begin(),
      dictionary->GetWords().end(),
      word);
}

SpellcheckCustomDictionary* GetDictionary(int index) {
  return SpellcheckServiceFactory::GetForProfile(
      sync_datatype_helper::test()->GetProfile(index))->GetCustomDictionary();
}

SpellcheckCustomDictionary* GetVerifierDictionary() {
  return SpellcheckServiceFactory::GetForProfile(
      sync_datatype_helper::test()->verifier())->GetCustomDictionary();
}

bool HaveWord(int index, std::string word) {
  return DictionaryHasWord(GetDictionary(index), word);
}

bool HaveWordInVerifier(std::string word) {
  return DictionaryHasWord(GetVerifierDictionary(), word);
}

void LoadDictionary(SpellcheckCustomDictionary* dictionary) {
  if (dictionary->IsLoaded())
    return;
  base::RunLoop run_loop;
  DictionaryLoadObserver observer(content::GetQuitTaskForRunLoop(&run_loop));
  dictionary->AddObserver(&observer);
  dictionary->Load();
  content::RunThisRunLoop(&run_loop);
  dictionary->RemoveObserver(&observer);
  ASSERT_TRUE(dictionary->IsLoaded());
}

bool HaveWordMatches(const std::string& word) {
  bool reference = sync_datatype_helper::test()->use_verifier() ?
      HaveWordInVerifier(word) : HaveWord(0, word);
  for (int i = 0; i < sync_datatype_helper::test()->num_clients(); ++i) {
    if (reference != HaveWord(i, word)) {
      return false;
    }
  }
  return true;
}

}  // namespace


void EnableDictionarySync(CommandLine* cl) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableSyncDictionary);
}

void LoadDictionaries() {
  for (int i = 0; i < sync_datatype_helper::test()->num_clients(); ++i)
    LoadDictionary(GetDictionary(i));
  if (sync_datatype_helper::test()->use_verifier())
    LoadDictionary(GetVerifierDictionary());
}

size_t GetDictionarySize(int index) {
  return GetDictionary(index)->GetWords().size();
}

size_t GetVerifierDictionarySize() {
  return GetVerifierDictionary()->GetWords().size();
}

bool DictionariesMatch() {
  chrome::spellcheck_common::WordList reference =
      sync_datatype_helper::test()->use_verifier() ?
      GetVerifierDictionary()->GetWords() : GetDictionary(0)->GetWords();
  for (int i = 0; i < sync_datatype_helper::test()->num_clients(); ++i) {
    if (reference.size() != GetDictionary(i)->GetWords().size())
      return false;
  }
  for (chrome::spellcheck_common::WordList::iterator it = reference.begin();
       it != reference.end();
       ++it) {
    if (!HaveWordMatches(*it))
      return false;
  }
  return true;
}

bool DictionaryMatchesVerifier(int index) {
  chrome::spellcheck_common::WordList expected =
      GetVerifierDictionary()->GetWords();
  chrome::spellcheck_common::WordList actual = GetDictionary(index)->GetWords();
  if (expected.size() != actual.size())
    return false;
  for (chrome::spellcheck_common::WordList::iterator it = expected.begin();
       it != expected.end();
       ++it) {
    if (actual.end() == std::find(actual.begin(), actual.end(), *it))
      return false;
  }
  return true;
}

bool AddWord(int index, const std::string& word) {
  SpellcheckCustomDictionary::Change dictionary_change;
  dictionary_change.AddWord(word);
  bool result = DictionarySyncIntegrationTestHelper::ApplyChange(
      GetDictionary(index), dictionary_change);
  if (sync_datatype_helper::test()->use_verifier()) {
    result &= DictionarySyncIntegrationTestHelper::ApplyChange(
        GetVerifierDictionary(), dictionary_change);
  }
  return result;
}

bool RemoveWord(int index, const std::string& word) {
  SpellcheckCustomDictionary::Change dictionary_change;
  dictionary_change.RemoveWord(word);
  bool result = DictionarySyncIntegrationTestHelper::ApplyChange(
      GetDictionary(index), dictionary_change);
  if (sync_datatype_helper::test()->use_verifier()) {
    result &= DictionarySyncIntegrationTestHelper::ApplyChange(
        GetVerifierDictionary(), dictionary_change);
  }
  return result;
}

}  // namespace dictionary_helper
