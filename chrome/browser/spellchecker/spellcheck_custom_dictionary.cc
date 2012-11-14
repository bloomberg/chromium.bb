// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/spellcheck_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

#include <functional>

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

void SpellcheckCustomDictionary::LoadDictionaryIntoCustomWordList(
    WordList* custom_words) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string contents;
  file_util::ReadFileToString(custom_dictionary_path_, &contents);
  if (contents.empty())
    return;

  base::SplitString(contents, '\n', custom_words);
  // Clear out empty words.
  custom_words->erase(remove_if(custom_words->begin(), custom_words->end(),
    mem_fun_ref(&std::string::empty)), custom_words->end());
}

void SpellcheckCustomDictionary::Load() {
  custom_words_.clear();
  // We are not guaranteed to be on the FILE thread so post the task.
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, base::Bind(
      &SpellcheckCustomDictionary::LoadDictionaryIntoCustomWordList,
      base::Unretained(this), &custom_words_));
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

void SpellcheckCustomDictionary::CustomWordAddedLocally(
    const std::string& word) {
  custom_words_.push_back(word);
  // TODO(rlp): record metrics on custom word size
}

bool SpellcheckCustomDictionary::SetCustomWordList(WordList* custom_words) {
  if (!custom_words)
    return false;
  custom_words_.clear();
  std::swap(custom_words_, *custom_words);
  return true;
}

const WordList& SpellcheckCustomDictionary::GetCustomWords() const {
  return custom_words_;
}

void SpellcheckCustomDictionary::AddWord(const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  CustomWordAddedLocally(word);

  BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
      base::Bind(&SpellcheckCustomDictionary::WriteWordToCustomDictionary,
                 base::Unretained(this), word),
      base::Bind(&SpellcheckCustomDictionary::AddWordComplete,
                 weak_ptr_factory_.GetWeakPtr(), word));
}

void SpellcheckCustomDictionary::AddWordComplete(const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->Send(new SpellCheckMsg_WordAdded(word));
  }
}


