// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/in_memory_url_index.h"

#include <algorithm>
#include <limits>

#include "app/l10n_util.h"
#include "base/i18n/word_iterator.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/url_database.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"

using base::Time;
using base::TimeDelta;

namespace history {

InMemoryURLIndex::InMemoryURLIndex() : history_item_count_(0) {}

InMemoryURLIndex::~InMemoryURLIndex() {}

// Indexing

bool InMemoryURLIndex::Init(history::URLDatabase* history_db,
                            const std::string& languages) {
  // TODO(mrossetti): Register for profile/language change notifications.
  languages_ = languages;
  // Reset our indexes.
  char_word_map_.clear();
  word_id_history_map_.clear();
  if (!history_db)
    return false;
  URLDatabase::URLEnumerator history_enum;
  if (history_db->InitURLEnumeratorForEverything(&history_enum)) {
    URLRow row;
    Time recent_threshold = InMemoryURLIndex::RecentThreshold();
    while (history_enum.GetNextURL(&row)) {
      // Do some filtering so that we only get history items which could
      // possibly pass the HistoryURLProvider::CullPoorMatches filter later.
      if ((row.typed_count() > kLowQualityMatchTypedLimit) ||
          (row.visit_count() > kLowQualityMatchVisitLimit) ||
          (row.last_visit() >= recent_threshold)) {
        if (!IndexRow(row))
          return false;
      }
    }
  }
  return true;
}

bool InMemoryURLIndex::IndexRow(URLRow row) {
  const GURL& gurl(row.url());
  string16 url(net::FormatUrl(gurl, languages_,
      net::kFormatUrlOmitUsernamePassword,
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS,
      NULL, NULL, NULL));

  // TODO(mrossetti): Find or implement a ConvertPercentEncoding and use it
  // on the url.

  // TODO(mrossetti): Detect row_id > std::numeric_limits<HistoryID>::max().
  HistoryID history_id = static_cast<HistoryID>(row.id());

  // Add the row for quick lookup in the history info store.
  url = l10n_util::ToLower(url);
  URLRow new_row(GURL(url), row.id());
  new_row.set_visit_count(row.visit_count());
  new_row.set_typed_count(row.typed_count());
  new_row.set_last_visit(row.last_visit());
  new_row.set_title(row.title());
  history_info_map_.insert(std::make_pair(history_id, new_row));

  // Split into individual, unique words.
  String16Set words = WordsFromString16(url);

  // For each word, add a new entry into the word index referring to the
  // associated history item.
  for (String16Set::iterator iter = words.begin();
       iter != words.end(); ++iter) {
    String16Set::value_type uni_word = *iter;
    AddWordToIndex(uni_word, history_id);
  }
  ++history_item_count_;
  return true;
}

// Utility Functions

InMemoryURLIndex::String16Set InMemoryURLIndex::WordsFromString16(
    const string16& uni_string) {
  String16Set words;

  // TODO(mrossetti): Replace all | and _'s with a space, all % quoted
  // characters with real characters, and break into words, using
  // appropriate string16 functions.
  WordIterator iter(&uni_string, WordIterator::BREAK_WORD);
  if (iter.Init()) {
    while (iter.Advance()) {
      if (iter.IsWord()) {
        words.insert(iter.GetWord());
      }
    }
  }
  return words;
}

InMemoryURLIndex::Char16Set InMemoryURLIndex::CharactersFromString16(
    const string16& uni_word) {
  Char16Set characters;
  for (string16::const_iterator iter = uni_word.begin();
       iter != uni_word.end(); ++iter)
    characters.insert(*iter);
  return characters;
}

void InMemoryURLIndex::AddWordToIndex(const string16& uni_word,
                                      HistoryID history_id) {
  WordMap::iterator word_pos = word_map_.find(uni_word);
  if (word_pos != word_map_.end())
    UpdateWordHistory(word_pos->second, history_id);
  else
    AddWordHistory(uni_word, history_id);
}

void InMemoryURLIndex::UpdateWordHistory(WordID word_id, HistoryID history_id) {
    WordIDHistoryMap::iterator history_pos = word_id_history_map_.find(word_id);
    DCHECK(history_pos != word_id_history_map_.end());
    HistoryIDSet& history_id_set(history_pos->second);
    history_id_set.insert(history_id);
}

// Add a new word to the word list and the word map, and then create a
// new entry in the word/history map.
void InMemoryURLIndex::AddWordHistory(const string16& uni_word,
                                      HistoryID history_id) {
  word_list_.push_back(uni_word);
  WordID word_id = word_list_.size() - 1;
  word_map_.insert(std::make_pair(uni_word, word_id));
  HistoryIDSet history_id_set;
  history_id_set.insert(history_id);
  word_id_history_map_.insert(std::make_pair(word_id, history_id_set));
  // For each character in the newly added word (i.e. a word that is not
  // already in the word index), add the word to the character index.
  Char16Set characters = CharactersFromString16(uni_word);
  for (Char16Set::iterator uni_char_iter = characters.begin();
       uni_char_iter != characters.end(); ++uni_char_iter) {
    Char16Set::value_type uni_string = *uni_char_iter;
    CharWordIDMap::iterator char_iter = char_word_map_.find(uni_string);
    if (char_iter != char_word_map_.end()) {
      // Update existing entry in the char/word index.
      WordIDSet& word_id_set(char_iter->second);
      word_id_set.insert(word_id);
    } else {
      // Create a new entry in the char/word index.
      WordIDSet word_id_set;
      word_id_set.insert(word_id);
      char_word_map_.insert(std::make_pair(uni_string, word_id_set));
    }
  }
}

// static
Time InMemoryURLIndex::RecentThreshold() {
  return Time::Now() - TimeDelta::FromDays(kLowQualityMatchAgeLimitInDays);
}

}  // namespace history
