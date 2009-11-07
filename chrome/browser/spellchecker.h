// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_H_
#define CHROME_BROWSER_SPELLCHECKER_H_

#include <queue>
#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "base/string16.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/spellcheck_worditerator.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_member.h"
#include "unicode/uscript.h"

class FilePath;
class Hunspell;
class PrefService;
class Profile;
class URLFetcher;
class URLRequestContextGetter;

namespace file_util {
class MemoryMappedFile;
}

// The Browser's Spell Checker. It checks and suggests corrections.
//
// This object is not threadsafe. In normal usage (not unit tests) it lives on
// the I/O thread of the browser. It is threadsafe refcounted so that I/O thread
// and the profile on the main thread (which gives out references to it) can
// keep it. However, all usage of this must be on the I/O thread.
//
// This object should also be deleted on the I/O thread only. It owns a
// reference to URLRequestContext which in turn owns the cache, etc. and must be
// deleted on the I/O thread itself.
class SpellChecker
    : public base::RefCountedThreadSafe<
          SpellChecker, ChromeThread::DeleteOnIOThread>,
      public URLFetcher::Delegate {
 public:
  // Creates the spellchecker by reading dictionaries from the given directory,
  // and defaulting to the given language. Both strings must be provided.
  //
  // The request context is used to download dictionaries if they do not exist.
  // This can be NULL if you don't want this (like in tests).
  // The |custom_dictionary_file_name| should be left blank so that Spellchecker
  // can figure out the custom dictionary file. It is non empty only for unit
  // testing.
  SpellChecker(const FilePath& dict_dir,
               const std::string& language,
               URLRequestContextGetter* request_context_getter,
               const FilePath& custom_dictionary_file_name);

  // SpellCheck a word.
  // Returns true if spelled correctly, false otherwise.
  // If the spellchecker failed to initialize, always returns true.
  // The |tag| parameter should either be a unique identifier for the document
  // that the word came from (if the current platform requires it), or 0.
  // In addition, finds the suggested words for a given word
  // and puts them into |*optional_suggestions|.
  // If the word is spelled correctly, the vector is empty.
  // If optional_suggestions is NULL, suggested words will not be looked up.
  // Note that Doing suggest lookups can be slow.
  bool SpellCheckWord(const char16* in_word,
                      int in_word_len,
                      int tag,
                      int* misspelling_start,
                      int* misspelling_len,
                      std::vector<string16>* optional_suggestions);

  // Find a possible correctly spelled word for a misspelled word. Computes an
  // empty string if input misspelled word is too long, there is ambiguity, or
  // the correct spelling cannot be determined.
  string16 GetAutoCorrectionWord(const string16& word, int tag);

  // Turn auto spell correct support ON or OFF.
  // |turn_on| = true means turn ON; false means turn OFF.
  void EnableAutoSpellCorrect(bool turn_on);

  // Add custom word to the dictionary, which means:
  //    a) Add it to the current hunspell object for immediate use,
  //    b) Add the word to a file in disk for custom dictionary.
  void AddWord(const string16& word);

  // Get SpellChecker supported languages.
  static void SpellCheckLanguages(std::vector<std::string>* languages);

  // This function computes a vector of strings which are to be displayed in
  // the context menu over a text area for changing spell check languages. It
  // returns the index of the current spell check language in the vector.
  // TODO(port): this should take a vector of string16, but the implementation
  // has some dependencies in l10n util that need porting first.
  static int GetSpellCheckLanguages(
      Profile* profile,
      std::vector<std::string>* languages);

  // This function returns the corresponding language-region code for the
  // spell check language. For example, for hi, it returns hi-IN.
  static std::string GetSpellCheckLanguageRegion(std::string input_language);

  // This function returns ll (language code) from ll-RR where 'RR' (region
  // code) is redundant. However, if the region code matters, it's preserved.
  // That is, it returns 'hi' and 'en-GB' for 'hi-IN' and 'en-GB' respectively.
  static std::string GetLanguageFromLanguageRegion(std::string input_language);

 private:
  friend class ChromeThread;
  friend class DeleteTask<SpellChecker>;
  friend class ReadDictionaryTask;
  FRIEND_TEST(SpellCheckTest, SpellCheckStrings_EN_US);
  FRIEND_TEST(SpellCheckTest, SpellCheckSuggestions_EN_US);
  FRIEND_TEST(SpellCheckTest, SpellCheckText);
  FRIEND_TEST(SpellCheckTest, DISABLED_SpellCheckAddToDictionary_EN_US);
  FRIEND_TEST(SpellCheckTest,
              DISABLED_SpellCheckSuggestionsAddToDictionary_EN_US);
  FRIEND_TEST(SpellCheckTest, GetAutoCorrectionWord_EN_US);
  FRIEND_TEST(SpellCheckTest, IgnoreWords_EN_US);

  // Only delete on the I/O thread (see above).
  ~SpellChecker();

  // URLFetcher::Delegate implementation.  Called when we finish downloading the
  // spellcheck dictionary; saves the dictionary to disk.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // When called, relays the request to check the spelling to the proper
  // backend, either hunspell or a platform-specific backend.
  bool CheckSpelling(const string16& word_to_check, int tag);

  // When called, relays the request to fill the list with suggestions to
  // the proper backend, either hunspell or a platform-specific backend.
  void FillSuggestionList(const string16& wrong_word,
                          std::vector<string16>* optional_suggestions);

  // Initializes the Hunspell Dictionary.
  bool Initialize();

  // Called when |hunspell| is done loading, succesfully or not. If |hunspell|
  // and |bdict_file| are non-NULL, assume ownership.
  void HunspellInited(Hunspell* hunspell,
                      file_util::MemoryMappedFile* bdict_file,
                      bool file_existed);

  // Either start downloading a dictionary if we have not already, or do nothing
  // if we have already tried to download one.
  void DoDictionaryDownload();

  // Returns whether or not the given word is a contraction of valid words
  // (e.g. "word:word").
  bool IsValidContraction(const string16& word, int tag);

  // Return the file name of the dictionary, including the path and the version
  // numbers.
  FilePath GetVersionedFileName(const std::string& language,
                                const FilePath& dict_dir);

  static std::string GetCorrespondingSpellCheckLanguage(
      const std::string& language);

  // Start downloading a dictionary from the server.  On completion, the
  // OnURLFetchComplete() function is invoked.
  void StartDictionaryDownload(const FilePath& file_name);

  // This method is called in the IO thread after dictionary download has
  // completed in FILE thread.
  void OnDictionarySaveComplete();

  // The given path to the directory whether SpellChecker first tries to
  // download the spellcheck bdic dictionary file.
  FilePath given_dictionary_directory_;

  // Path to the custom dictionary file.
  FilePath custom_dictionary_file_name_;

  // BDIC file name (e.g. en-US_1_1.bdic).
  FilePath bdic_file_name_;

  // We memory-map the BDict file.
  scoped_ptr<file_util::MemoryMappedFile> bdict_file_;

  // The hunspell dictionary in use.
  scoped_ptr<Hunspell> hunspell_;

  // Represents character attributes used for filtering out characters which
  // are not supported by this SpellChecker object.
  SpellcheckCharAttribute character_attributes_;

  // Flag indicating whether we've tried to initialize.  If we've already
  // attempted initialiation, we won't retry to avoid failure loops.
  bool tried_to_init_;

  // The language that this spellchecker works in.
  std::string language_;

  // This object must only be used on the same thread. However, it is normally
  // created on the UI thread. This checks calls to SpellCheckWord and the
  // destructor to make sure we're only ever running on the same thread.
  //
  // This will be NULL if it is not initialized yet (not initialized in the
  // constructor since that's on a different thread).
  MessageLoop* worker_loop_;

  // Flag indicating whether we tried to download the dictionary file.
  bool tried_to_download_dictionary_file_;

  // Used for requests. MAY BE NULL which means don't try to download.
  URLRequestContextGetter* request_context_getter_;

  // True when we're downloading or saving a dictionary.
  bool obtaining_dictionary_;

  // Remember state for auto spell correct.
  bool auto_spell_correct_turned_on_;

  // True if a platform-specific spellchecking engine is being used,
  // and False if hunspell is being used.
  bool is_using_platform_spelling_engine_;

  // URLFetcher to download a file in memory.
  scoped_ptr<URLFetcher> fetcher_;

  // While Hunspell is loading, we add any new custom words to this queue.
  // We will add them to |hunspell_| when it is done loading.
  std::queue<std::string> custom_words_;

  // Used for generating callbacks to spellchecker, since spellchecker is a
  // non-reference counted object.
  ScopedRunnableMethodFactory<SpellChecker> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(SpellChecker);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_H_
