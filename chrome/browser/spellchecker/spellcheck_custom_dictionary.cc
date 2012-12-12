// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"

#include <functional>

#include "base/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/md5.h"
#include "base/string_split.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/spellcheck_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;
using chrome::spellcheck_common::WordList;

namespace {

const FilePath::CharType BACKUP_EXTENSION[] = FILE_PATH_LITERAL("backup");
const char CHECKSUM_PREFIX[] = "checksum_v1 = ";

// Loads the lines from the file at |file_path| into the |lines| container. If
// the file has a valid checksum, then returns |true|. If the file has an
// invalid checksum, then returns |false| and clears |lines|.
bool LoadFile(FilePath file_path, std::vector<std::string>* lines) {
  lines->clear();
  std::string contents;
  file_util::ReadFileToString(file_path, &contents);
  size_t pos = contents.rfind(CHECKSUM_PREFIX);
  if (pos != std::string::npos) {
    std::string checksum = contents.substr(pos + strlen(CHECKSUM_PREFIX));
    contents = contents.substr(0, pos);
    if (checksum != base::MD5String(contents))
      return false;
  }
  TrimWhitespaceASCII(contents, TRIM_ALL, &contents);
  base::SplitString(contents, '\n', lines);
  return true;
}

bool IsValidWord(const std::string& word) {
  return IsStringUTF8(word) && word.length() <= 128 && word.length() > 0 &&
      std::string::npos == word.find_first_of(kWhitespaceASCII);
}

bool IsInvalidWord(const std::string& word) {
  return !IsValidWord(word);
}

}  // namespace

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

  LoadDictionaryFileReliably(custom_words);
  if (custom_words->empty())
    return;

  // Clean up the dictionary file contents by removing duplicates and invalid
  // words.
  std::sort(custom_words->begin(), custom_words->end());
  custom_words->erase(std::unique(custom_words->begin(), custom_words->end()),
                      custom_words->end());
  custom_words->erase(std::remove_if(custom_words->begin(),
                                     custom_words->end(),
                                     IsInvalidWord),
                      custom_words->end());

  SaveDictionaryFileReliably(*custom_words);
}

void SpellcheckCustomDictionary::SetCustomWordList(WordList* custom_words) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  words_.clear();
  if (custom_words)
    std::swap(words_, *custom_words);

  FOR_EACH_OBSERVER(Observer, observers_, OnCustomDictionaryLoaded());
}

bool SpellcheckCustomDictionary::AddWord(const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!IsValidWord(word))
    return false;

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

  FOR_EACH_OBSERVER(Observer, observers_, OnCustomDictionaryWordAdded(word));

  return true;
}

bool SpellcheckCustomDictionary::CustomWordAddedLocally(
    const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(IsValidWord(word));

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
  DCHECK(IsValidWord(word));

  WordList custom_words;
  LoadDictionaryFileReliably(&custom_words);
  custom_words.push_back(word);
  SaveDictionaryFileReliably(custom_words);
}

bool SpellcheckCustomDictionary::RemoveWord(const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!IsValidWord(word))
    return false;

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

  FOR_EACH_OBSERVER(Observer, observers_, OnCustomDictionaryWordRemoved(word));

  return true;
}

bool SpellcheckCustomDictionary::CustomWordRemovedLocally(
    const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(IsValidWord(word));

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
  DCHECK(IsValidWord(word));

  WordList custom_words;
  LoadDictionaryFileReliably(&custom_words);
  if (custom_words.empty())
    return;

  WordList::iterator it = std::find(custom_words.begin(),
                                    custom_words.end(),
                                    word);
  if (it != custom_words.end())
    custom_words.erase(it);

  SaveDictionaryFileReliably(custom_words);
}

void SpellcheckCustomDictionary::AddObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  observers_.AddObserver(observer);
}

void SpellcheckCustomDictionary::RemoveObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  observers_.RemoveObserver(observer);
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

void SpellcheckCustomDictionary::LoadDictionaryFileReliably(
    WordList* custom_words) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Load the contents and verify the checksum.
  if (LoadFile(custom_dictionary_path_, custom_words))
    return;

  // Checksum is not valid. See if there's a backup.
  FilePath backup = custom_dictionary_path_.AddExtension(BACKUP_EXTENSION);
  if (!file_util::PathExists(backup))
    return;

  // Load the backup and verify its checksum.
  if (!LoadFile(backup, custom_words))
    return;

  // Backup checksum is valid. Restore the backup.
  file_util::CopyFile(backup, custom_dictionary_path_);
}

void SpellcheckCustomDictionary::SaveDictionaryFileReliably(
    const WordList& custom_words) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::stringstream content;
  for (WordList::const_iterator it = custom_words.begin();
       it != custom_words.end();
       ++it) {
    content << *it << '\n';
  }
  std::string checksum = base::MD5String(content.str());
  content << CHECKSUM_PREFIX << checksum;

  file_util::CopyFile(custom_dictionary_path_,
                      custom_dictionary_path_.AddExtension(BACKUP_EXTENSION));
  base::ImportantFileWriter::WriteFileAtomically(custom_dictionary_path_,
                                                 content.str());
}
