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
  explicit SpellcheckCustomDictionary(Profile* profile);
  virtual ~SpellcheckCustomDictionary();

  virtual void Load() OVERRIDE;
  void WriteWordToCustomDictionary(const std::string& word);

  // Returns true if successful. False otherwise. Takes ownership of
  // custom_words and replaces pointer with an empty vector.
  bool SetCustomWordList(chrome::spellcheck_common::WordList* custom_words);
  const chrome::spellcheck_common::WordList& GetCustomWords() const;
  void CustomWordAddedLocally(const std::string& word);
  void LoadDictionaryIntoCustomWordList(
      chrome::spellcheck_common::WordList* custom_words);

  // Adds the given word to the custom words list and inform renderer of the
  // update.
  void AddWord(const std::string& word);

  // The reply point for PostTaskAndReply. Called when AddWord is finished
  // adding a word in the background.
  void AddWordComplete(const std::string& word);

 private:
  // In-memory cache of the custom words file.
  chrome::spellcheck_common::WordList custom_words_;

  // A path for custom dictionary per profile.
  FilePath custom_dictionary_path_;

  base::WeakPtrFactory<SpellcheckCustomDictionary> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckCustomDictionary);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_CUSTOM_DICTIONARY_H_
