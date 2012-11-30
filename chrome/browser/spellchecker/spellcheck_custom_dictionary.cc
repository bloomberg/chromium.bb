// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"

#include <functional>

#include "base/file_util.h"
#include "base/string_split.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/spellcheck_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;
using chrome::spellcheck_common::WordList;

SpellcheckCustomDictionary::SpellcheckCustomDictionary(Profile* profile)
    : SpellcheckDictionary(profile),
      custom_dictionary_path_(),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(profile);
  custom_dictionary_path_ =
      profile_->GetPath().Append(chrome::kCustomDictionaryFileName);
}

SpellcheckCustomDictionary::~SpellcheckCustomDictionary() {
}

void SpellcheckCustomDictionary::Load() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTaskAndReplyWithResult<WordList*>(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&SpellcheckCustomDictionary::LoadDictionary,
                 base::Unretained(this)),
      base::Bind(&SpellcheckCustomDictionary::SetCustomWordListAndDelete,
                 weak_ptr_factory_.GetWeakPtr()));
}

const WordList& SpellcheckCustomDictionary::GetWords() const {
  return words_;
}

void SpellcheckCustomDictionary::LoadDictionaryIntoCustomWordList(
    WordList* custom_words) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string contents;
  file_util::ReadFileToString(custom_dictionary_path_, &contents);
  if (contents.empty()) {
    custom_words->clear();
    return;
  }

  base::SplitString(contents, '\n', custom_words);

  // Erase duplicates.
  std::sort(custom_words->begin(), custom_words->end());
  custom_words->erase(std::unique(custom_words->begin(), custom_words->end()),
                      custom_words->end());

  // Clear out empty words.
  custom_words->erase(remove_if(custom_words->begin(), custom_words->end(),
    mem_fun_ref(&std::string::empty)), custom_words->end());

  // Write out the clean file.
  std::stringstream ss;
  for (WordList::iterator it = custom_words->begin();
       it != custom_words->end();
       ++it) {
    ss << *it << '\n';
  }
  contents = ss.str();
  file_util::WriteFile(custom_dictionary_path_,
                       contents.c_str(),
                       contents.length());
}

void SpellcheckCustomDictionary::SetCustomWordList(WordList* custom_words) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  words_.clear();
  if (custom_words)
    std::swap(words_, *custom_words);

  std::vector<Observer*>::iterator it;
  for (it = observers_.begin(); it != observers_.end(); ++it)
    (*it)->OnCustomDictionaryLoaded();
}

bool SpellcheckCustomDictionary::AddWord(const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!CustomWordAddedLocally(word))
    return false;

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&SpellcheckCustomDictionary::WriteWordToCustomDictionary,
                 base::Unretained(this), word));

  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->Send(new SpellCheckMsg_WordAdded(word));
  }

  std::vector<Observer*>::iterator it;
  for (it = observers_.begin(); it != observers_.end(); ++it)
    (*it)->OnCustomDictionaryWordAdded(word);

  return true;
}

bool SpellcheckCustomDictionary::CustomWordAddedLocally(
    const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  WordList::iterator it = std::find(words_.begin(), words_.end(), word);
  if (it == words_.end()) {
    words_.push_back(word);
    return true;
  }
  return false;
  // TODO(rlp): record metrics on custom word size
}

void SpellcheckCustomDictionary::WriteWordToCustomDictionary(
    const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Stored in UTF-8.
  DCHECK(IsStringUTF8(word));

  std::string word_to_add(word + "\n");
  if (!file_util::PathExists(custom_dictionary_path_)) {
    file_util::WriteFile(custom_dictionary_path_, word_to_add.c_str(),
        word_to_add.length());
  } else {
    file_util::AppendToFile(custom_dictionary_path_, word_to_add.c_str(),
        word_to_add.length());
  }
}

bool SpellcheckCustomDictionary::RemoveWord(const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!CustomWordRemovedLocally(word))
    return false;

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&SpellcheckCustomDictionary::EraseWordFromCustomDictionary,
                 base::Unretained(this), word));

  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->Send(new SpellCheckMsg_WordRemoved(word));
  }

  std::vector<Observer*>::iterator it;
  for (it = observers_.begin(); it != observers_.end(); ++it)
    (*it)->OnCustomDictionaryWordRemoved(word);

  return true;
}

bool SpellcheckCustomDictionary::CustomWordRemovedLocally(
    const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  WordList::iterator it = std::find(words_.begin(), words_.end(), word);
  if (it != words_.end()) {
    words_.erase(it);
    return true;
  }
  return false;
}

void SpellcheckCustomDictionary::EraseWordFromCustomDictionary(
    const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(IsStringUTF8(word));

  WordList custom_words;
  LoadDictionaryIntoCustomWordList(&custom_words);

  const char empty[] = {'\0'};
  const char separator[] = {'\n', '\0'};
  file_util::WriteFile(custom_dictionary_path_, empty, 0);
  for (WordList::iterator it = custom_words.begin();
      it != custom_words.end();
      ++it) {
    std::string word_to_add = *it;
    if (word.compare(word_to_add) != 0) {
      file_util::AppendToFile(custom_dictionary_path_, word_to_add.c_str(),
          word_to_add.length());
      file_util::AppendToFile(custom_dictionary_path_, separator, 1);
    }
  }
}

void SpellcheckCustomDictionary::AddObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  observers_.push_back(observer);
}

void SpellcheckCustomDictionary::RemoveObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<Observer*>::iterator it = std::find(observers_.begin(),
                                                  observers_.end(),
                                                  observer);
  if (it != observers_.end())
    observers_.erase(it);
}

WordList* SpellcheckCustomDictionary::LoadDictionary() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  WordList* custom_words = new WordList;
  LoadDictionaryIntoCustomWordList(custom_words);
  return custom_words;
}

void SpellcheckCustomDictionary::SetCustomWordListAndDelete(
    WordList* custom_words) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SetCustomWordList(custom_words);
  delete custom_words;
}
