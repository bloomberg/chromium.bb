// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_CUSTOM_DICTIONARY_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_CUSTOM_DICTIONARY_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/spellchecker/spellcheck_dictionary.h"
#include "chrome/common/spellcheck_common.h"

// Defines a custom dictionary which users can add their own words to.
class SpellcheckCustomDictionary : public SpellcheckDictionary {
 public:
  class Observer {
   public:
    virtual void OnCustomDictionaryLoaded() = 0;
    virtual void OnCustomDictionaryWordAdded(const std::string& word) = 0;
    virtual void OnCustomDictionaryWordRemoved(const std::string& word) = 0;
  };

  explicit SpellcheckCustomDictionary(Profile* profile);
  virtual ~SpellcheckCustomDictionary();

  // Overridden from SpellcheckDictionary:
  virtual void Load() OVERRIDE;

  const chrome::spellcheck_common::WordList& GetWords() const;

  // Populates the |custom_words| with the list of words in the custom
  // dictionary file. Makes sure that the custom dictionary file is sorted and
  // does not have duplicates.
  void LoadDictionaryIntoCustomWordList(
      chrome::spellcheck_common::WordList* custom_words);

  // Moves the words from the |custom_words| argument into its own private
  // member variable. Does not delete the memory at |custom_words|.
  void SetCustomWordList(chrome::spellcheck_common::WordList* custom_words);

  // Adds the given word to the custom words list and inform renderer of the
  // update. Returns false for duplicate words.
  bool AddWord(const std::string& word);

  // Returns false for duplicate words.
  bool CustomWordAddedLocally(const std::string& word);

  void WriteWordToCustomDictionary(const std::string& word);

  // Removes the given word from the custom words list and informs renderer of
  // the update. Returns false for words that are not in the dictionary.
  bool RemoveWord(const std::string& word);

  // Returns false for words that are not in the dictionary.
  bool CustomWordRemovedLocally(const std::string& word);

  void EraseWordFromCustomDictionary(const std::string& word);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Returns a newly allocated list of words read from custom dictionary file.
  // The caller owns this new list.
  chrome::spellcheck_common::WordList* LoadDictionary();

  // The reply point for PostTaskAndReplyWithResult. Called when LoadDictionary
  // is finished reading words from custom dictionary file. Moves the strings
  // from the |custom_words| argument into the private member variable |words_|
  // and deletes the memory at |custom_words|.
  void SetCustomWordListAndDelete(
      chrome::spellcheck_common::WordList* custom_words);

  // In-memory cache of the custom words file.
  chrome::spellcheck_common::WordList words_;

  // A path for custom dictionary per profile.
  FilePath custom_dictionary_path_;

  base::WeakPtrFactory<SpellcheckCustomDictionary> weak_ptr_factory_;

  std::vector<Observer*> observers_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckCustomDictionary);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_CUSTOM_DICTIONARY_H_
