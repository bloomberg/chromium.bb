// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_INDEX_H_
#define CHROME_BROWSER_HISTORY_HISTORY_INDEX_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#if defined(USE_SYSTEM_SQLITE)
#include <sqlite3.h>
#else
#include "third_party/sqlite/preprocessed/sqlite3.h"
#endif

#include "third_party/icu/public/common/unicode/rbbi.h"
#include "third_party/icu/public/i18n/unicode/regex.h"
#include "third_party/icu/public/common/unicode/unistr.h"

#define HISTORY_INDEX_STATISTICS_ENABLED 1

// Convenience types
typedef std::vector<icu::UnicodeString> UnicodeStringVector;
typedef std::set<icu::UnicodeString> UnicodeStringSet;
typedef std::set<UChar> UCharSet;
typedef std::vector<std::string> SearchTermsVector;

// An index into list of all of the words we have indexed.
typedef uint16_t WordID;

// A map allowing a WordID to be determined given a word.
typedef std::map<icu::UnicodeString, WordID> WordMap;

// A map from character to word_id's.
typedef std::set<WordID> WordIDSet;  // An index into the WordList.
typedef std::map<UChar, WordIDSet> CharWordIDMap;

// A map from word_id to history item.
typedef uint16_t HistoryID;  // uint16_t might actually work for us.
typedef std::set<HistoryID> HistoryIDSet;
typedef std::map<WordID, HistoryIDSet> WordIDHistoryMap;

// Support caching of term character intersections so that we can optimize
// searches which build upon a previous search.
struct TermCharWordSet {
  UCharSet char_set_;
  WordIDSet word_id_set_;
  bool used_;  // true if this set has been used for the current term search.
  TermCharWordSet(UCharSet char_set, WordIDSet word_id_set, bool used)
      : char_set_(char_set), word_id_set_(word_id_set), used_(used) { }
};
typedef std::vector<TermCharWordSet> TermCharWordSetVector;

// Search results base class.
struct HistoryItem {
  HistoryID history_id_;
  std::string url_;
  std::string title_;
  int visit_count_;
  std::string last_visit_time_;
  // TODO(mrossetti): Make a class. Add fields for score and to provide
  // term matching metrics so a UI can highlight the exact match.

  HistoryItem(HistoryID history_id,
              std::string& url,
              std::string& title,
              int visit_count,
              std::string& last_visit_time)
  : history_id_(history_id), url_(url), title_(title),
    visit_count_(visit_count), last_visit_time_(last_visit_time) { }

  // Return true if any of the search terms do not match a substring.
  // TODO(mrossetti): Also need to score and add UI hints.
  bool TermIsMissing(SearchTermsVector* terms_vector) const;
};
typedef std::vector<HistoryItem> HistoryItemVector;

// TODO(mrossetti): Rip out either the SQLite filtering or the in-memory
// HistoryInfo structure.
// A map from history_id to the history's URL and title.
typedef std::map<HistoryID, HistoryItem> HistoryInfoMap;

// The history source.
class HistoryIndex {
 public:
  HistoryIndex() : use_SQLite_filter_(false), history_database_(NULL),
      history_item_count_(0), question_matcher_(NULL), percent_matcher_(NULL),
      non_alnum_matcher_(NULL), bar_matcher_(NULL), word_breaker_(NULL)  { }
  ~HistoryIndex();

  // Open and index the URL history database.
  bool IndexURLHistory(std::string history_path);

  // Reset the history index.
  void Reset();

  // Given a vector containing one or more words as utf8 strings, scan the
  // history index and return a vector with all scored, matching history items.
  // Each term must occur somewhere in the history item for the item to
  // qualify, however, the terms do not necessarily have to be adjacent.
  HistoryItemVector HistoryItemsForTerms(SearchTermsVector const& terms);

  // NOTE: Temporary for timing and memory analysis.
  void SetUseSQLiteFilter(bool use_filter) { use_SQLite_filter_ = use_filter; }

#if HISTORY_INDEX_STATISTICS_ENABLED

  // Return a count of the history items which have been indexed.
  u_int64_t IndexedHistoryCount();

  // Return a count of the words which have been indexed.
  u_int64_t IndexedWordCount();

  // Return a count of the characters which have been indexed.
  u_int64_t IndexedCharacterCount();

  // Return a vector containing all of the words which have been indexed
  // as UTF8 strings.
  UnicodeStringVector IndexedWords() const;

  // Return the count of history items for the given word.
  u_int64_t NumberOfHistoryItemsForWord(icu::UnicodeString uni_word);

  // Return a vector containing all of the characters which have been indexed
  // as UTF8 strings.
  UnicodeStringVector IndexedCharacters();

  // Return the count of words for the given character.
  u_int64_t NumberOfWordsForChar(UChar uni_char);

  // Return the total size of the word list vector.
  u_int64_t WordListSize();

  // Return the total size of the word/word-id map.
  u_int64_t WordMapSize();

  // Return the total size of the word_id_history_map_.
  u_int64_t WordHistoryMapSize();

  // Return the total size of the char_word_map_.
  u_int64_t CharWordMapSize();

  // Return the total size of the char_word_map_.
  u_int64_t HistoryInfoSize();

#endif  // HISTORY_INDEX_STATISTICS_ENABLED

 private:
  // Index one URL history item.
  bool IndexRow(const char* id, const char* url, const char* title);

  // sqlite3 query callback used during initial history indexing.
  static int URLHistoryQueryHandler(void* context, int columns,
                                    char** column_strings,
                                    char** column_names);

  // Given a single search term (i.e. one word, utf8) return a set of
  // history IDs which map to that term but do not necessarily match
  // as a substring.
  HistoryIDSet HistoryIDsForTerm(icu::UnicodeString uni_word);

  // sqlite3 query callback used during final term filtering.
  static int URLTermQueryHandler(void* context, int columns,
                                 char** column_strings,
                                 char** column_names);

  // Break an UnicodeString down into individual words.
  UnicodeStringSet WordsFromUnicodeString(icu::UnicodeString uni_string);

  // Break an UnicodeString down into its individual characters.
  // Note that this is temporarily intended to work on a single word, but
  // _will_ work on a string of words, perhaps with unexpected results.
  // TODO(mrossetti): Lots of optimizations possible here for not restarting
  // a search if the user is just typing along. Also, change this to uniString
  // and properly handle substring matches, scoring and sorting the results
  // by score. Also, provide the metrics for where the matches occur so that
  // the UI can highlight the matched sections.
  UCharSet CharactersFromUnicodeString(icu::UnicodeString uni_word);

  // Given a set of UChars, find words containing those characters. If any
  // existing, cached set is a proper subset then start with that cached
  // set. Update the cache.
  WordIDSet WordIDSetForTermChars(UCharSet const& uni_chars);

 private:
  UnicodeStringVector word_list_;  // A list of all of indexed words.
                                   // NOTE: A word will _never_ be removed from
                                   // this vector to insure that the word_id
                                   // remains immutable throughout the session,
                                   // reducing maintenance complexity.
  bool use_SQLite_filter_;
  sqlite3* history_database_;
  u_int64_t history_item_count_;
  WordMap word_map_;
  CharWordIDMap char_word_map_;
  WordIDHistoryMap word_id_history_map_;
  TermCharWordSetVector term_char_word_set_cache_;
  HistoryInfoMap history_info_map_;

  // Cached items.
  icu::RegexMatcher *question_matcher_;
  icu::RegexMatcher *percent_matcher_;
  icu::RegexMatcher *non_alnum_matcher_;
  icu::RegexMatcher *bar_matcher_;
  icu::RuleBasedBreakIterator* word_breaker_;
};

#endif  // CHROME_BROWSER_HISTORY_HISTORY_INDEX_H_
