// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_H_
#define CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "app/sql/connection.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "third_party/icu/public/common/unicode/rbbi.h"
#include "third_party/icu/public/i18n/unicode/regex.h"
#include "third_party/icu/public/common/unicode/unistr.h"

namespace history {

// Define the following if your application would like to collect information
// about the history index and memory usage.
// #define HISTORY_INDEX_STATISTICS_ENABLED 1

// TODO(mrossetti): Move as many as possible of these typedefs, structs,
// and support classes into the InMemoryURLIndex class.

// UnicodeString compare functor which assists in handling UBool.
class UnicodeStringCompare {
 public:
  bool operator() (const icu::UnicodeString& lhs,
                   const icu::UnicodeString& rhs) {
    return (lhs < rhs) ? true : false;
  }
};

// Convenience types
typedef std::vector<icu::UnicodeString> UnicodeStringVector;
typedef std::set<icu::UnicodeString, UnicodeStringCompare> UnicodeStringSet;
typedef std::set<UChar> UCharSet;
typedef std::vector<std::string> SearchTermsVector;

// An index into list of all of the words we have indexed.
typedef int16 WordID;

// A map allowing a WordID to be determined given a word.
typedef std::map<icu::UnicodeString, WordID, UnicodeStringCompare> WordMap;

// A map from character to word_id's.
typedef std::set<WordID> WordIDSet;  // An index into the WordList.
typedef std::map<UChar, WordIDSet> CharWordIDMap;

// A map from word_id to history item.
typedef int16 HistoryID;  // uint16 might actually work for us.
typedef std::set<HistoryID> HistoryIDSet;
typedef std::map<WordID, HistoryIDSet> WordIDHistoryMap;

// Support caching of term character intersections so that we can optimize
// searches which build upon a previous search.
struct TermCharWordSet {
  TermCharWordSet(UCharSet char_set, WordIDSet word_id_set, bool used)
      : char_set_(char_set), word_id_set_(word_id_set), used_(used) {}

  UCharSet char_set_;
  WordIDSet word_id_set_;
  bool used_;  // true if this set has been used for the current term search.
};
typedef std::vector<TermCharWordSet> TermCharWordSetVector;

// Describes an entry in the urls table of the History database.
struct URLHistoryItem {
  URLHistoryItem(HistoryID history_id,
                 std::string& url,
                 std::string& title,
                 int visit_count,
                 std::string& last_visit_time)
      : history_id_(history_id), url_(url), title_(title),
        visit_count_(visit_count), last_visit_time_(last_visit_time) {}

  // Return true if any of the search terms do not match a substring.
  // TODO(mrossetti): Also need to score and add UI hints.
  bool TermIsMissing(SearchTermsVector* terms_vector) const;

  HistoryID history_id_;
  std::string url_;
  std::string title_;
  int visit_count_;
  std::string last_visit_time_;
  // TODO(mrossetti): Make a class. Add fields for score and to provide
  // term matching metrics so a UI can highlight the exact match.
};
typedef std::vector<URLHistoryItem> URLItemVector;

// TODO(mrossetti): Rip out either the SQLite filtering or the in-memory
// HistoryInfo structure.
// A map from history_id to the history's URL and title.
typedef std::map<HistoryID, URLHistoryItem> HistoryInfoMap;

// The history source.
class InMemoryURLIndex {
 public:
  InMemoryURLIndex()
      : history_item_count_(0), question_matcher_(NULL),
        percent_matcher_(NULL), non_alnum_matcher_(NULL), bar_matcher_(NULL),
        word_breaker_(NULL)  {}
  ~InMemoryURLIndex() {}

  // Open and index the URL history database.
  bool IndexURLHistory(const FilePath& history_path);

  // Reset the history index.
  void Reset();

  // Given a vector containing one or more words as utf8 strings, scan the
  // history index and return a vector with all scored, matching history items.
  // Each term must occur somewhere in the history item for the item to
  // qualify, however, the terms do not necessarily have to be adjacent.
  URLItemVector HistoryItemsForTerms(SearchTermsVector const& terms);

#if HISTORY_INDEX_STATISTICS_ENABLED

  // Return a count of the history items which have been indexed.
  uint64 IndexedHistoryCount();

  // Return a count of the words which have been indexed.
  uint64 IndexedWordCount();

  // Return a count of the characters which have been indexed.
  uint64 IndexedCharacterCount();

  // Return a vector containing all of the words which have been indexed
  // as UTF8 strings.
  UnicodeStringVector IndexedWords() const;

  // Return the count of history items for the given word.
  uint64 NumberOfHistoryItemsForWord(icu::UnicodeString uni_word);

  // Return a vector containing all of the characters which have been indexed
  // as UTF8 strings.
  UnicodeStringVector IndexedCharacters();

  // Return the count of words for the given character.
  uint64 NumberOfWordsForChar(UChar uni_char);

  // Return the total size of the word list vector.
  uint64 WordListSize();

  // Return the total size of the word/word-id map.
  uint64 WordMapSize();

  // Return the total size of the word_id_history_map_.
  uint64 WordHistoryMapSize();

  // Return the total size of the char_word_map_.
  uint64 CharWordMapSize();

  // Return the total size of the char_word_map_.
  uint64 HistoryInfoSize();

#endif  // HISTORY_INDEX_STATISTICS_ENABLED

 private:
  // Break an UnicodeString down into individual words.
  UnicodeStringSet WordsFromUnicodeString(icu::UnicodeString uni_string);

  // Given a set of UChars, find words containing those characters. If any
  // existing, cached set is a proper subset then start with that cached
  // set. Update the cache.
  WordIDSet WordIDSetForTermChars(UCharSet const& uni_chars);

  // URL History indexing support functions.

  // Index one URL history item.
  bool IndexRow(int64 row_id, std::string url, std::string title);

  // Break an UnicodeString down into its individual characters.
  // Note that this is temporarily intended to work on a single word, but
  // _will_ work on a string of words, perhaps with unexpected results.
  // TODO(mrossetti): Lots of optimizations possible here for not restarting
  // a search if the user is just typing along. Also, change this to uniString
  // and properly handle substring matches, scoring and sorting the results
  // by score. Also, provide the metrics for where the matches occur so that
  // the UI can highlight the matched sections.
  UCharSet CharactersFromUnicodeString(icu::UnicodeString uni_word);

  // Trim the parameters from the URL being indexed. Return true if there
  // were not errors encountered during the operation.
  bool TrimParameters(icu::UnicodeString& uni_string);

  // Convert %xx encodings in the URL being indexed. Return true if there
  // were not errors encountered during the operation.
  bool ConvertPercentEncoding(icu::UnicodeString& uni_string);

  // Given a single word, add a reference to the containing history item
  // to the index.
  void AddWordToIndex(icu::UnicodeString& uni_word, HistoryID history_id);

  // URL History index search support functions.

  // Given a single search term (i.e. one word, utf8) return a set of
  // history IDs which map to that term but do not necessarily match
  // as a substring.
  HistoryIDSet HistoryIDsForTerm(icu::UnicodeString uni_word);

  // Clear the search term cache. This cache holds on to the intermediate
  // word results for each previously typed character to eliminate the need
  // to re-perform set operations for previously typed characters.
  void ResetTermCharWordSetCache();

  // Compose a set of history item IDs by intersecting the set for each word
  // in |uni_string|.
  HistoryIDSet HistoryIDSetFromWords(icu::UnicodeString uni_string);

 private:
  UnicodeStringVector word_list_;  // A list of all of indexed words.
                                   // NOTE: A word will _never_ be removed from
                                   // this vector to insure that the word_id
                                   // remains immutable throughout the session,
                                   // reducing maintenance complexity.
  sql::Connection history_database_;
  uint64 history_item_count_;
  WordMap word_map_;
  CharWordIDMap char_word_map_;
  WordIDHistoryMap word_id_history_map_;
  TermCharWordSetVector term_char_word_set_cache_;
  HistoryInfoMap history_info_map_;

  // Cached items.
  scoped_ptr<icu::RegexMatcher> question_matcher_;
  scoped_ptr<icu::RegexMatcher> percent_matcher_;
  scoped_ptr<icu::RegexMatcher> non_alnum_matcher_;
  scoped_ptr<icu::RegexMatcher> bar_matcher_;
  scoped_ptr<icu::RuleBasedBreakIterator> word_breaker_;
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_H_
