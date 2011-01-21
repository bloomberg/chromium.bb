// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/in_memory_url_index.h"

#include <algorithm>
#include <limits>

#include "base/i18n/break_iterator.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/history_provider_util.h"
#include "chrome/browser/history/url_database.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "unicode/utypes.h"  // for int32_t

using base::Time;
using base::TimeDelta;

namespace history {

ScoredHistoryMatch::ScoredHistoryMatch() : raw_score(0) {}

ScoredHistoryMatch::ScoredHistoryMatch(const URLRow& url_info,
                                       size_t input_location,
                                       bool match_in_scheme,
                                       bool innermost_match,
                                       int score)
    : HistoryMatch(url_info, input_location, match_in_scheme, innermost_match),
      raw_score(score) {
}

struct InMemoryURLIndex::TermCharWordSet {
  TermCharWordSet(const Char16Set& char_set,
                  const WordIDSet& word_id_set,
                  bool used)
      : char_set_(char_set),
        word_id_set_(word_id_set),
        used_(used) {}

  bool IsNotUsed() const { return !used_; }

  Char16Set char_set_;
  WordIDSet word_id_set_;
  bool used_;  // true if this set has been used for the current term search.
};

InMemoryURLIndex::InMemoryURLIndex() : history_item_count_(0) {}

InMemoryURLIndex::~InMemoryURLIndex() {}

static const int32_t kURLToLowerBufferSize = 2048;

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

bool InMemoryURLIndex::IndexRow(const URLRow& row) {
  const GURL& gurl(row.url());
  string16 url(net::FormatUrl(gurl, languages_,
      net::kFormatUrlOmitUsernamePassword,
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS,
      NULL, NULL, NULL));

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
  String16Set words = WordSetFromString16(url);

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

// Searching

ScoredHistoryMatches InMemoryURLIndex::HistoryItemsForTerms(
    const String16Vector& terms) {
  ScoredHistoryMatches scored_items;
  if (!terms.empty()) {
    // Reset used_ flags for term_char_word_set_cache_. We use a basic mark-
    // and-sweep approach.
    ResetTermCharWordSetCache();

    // Lowercase the terms.
    // TODO(mrossetti): Another opportunity for a transform algorithm.
    String16Vector lower_terms;
    for (String16Vector::const_iterator term_iter = terms.begin();
         term_iter != terms.end(); ++term_iter) {
      String16Vector::value_type lower_string(*term_iter);
      std::transform(lower_string.begin(),
                     lower_string.end(),
                     lower_string.begin(),
                     tolower);
      lower_terms.push_back(lower_string);
    }

    String16Vector::value_type all_terms(JoinString(lower_terms, ' '));
    HistoryIDSet history_id_set = HistoryIDSetFromWords(all_terms);

    // Pass over all of the candidates filtering out any without a proper
    // substring match, inserting those which pass in order by score.
    scored_items = std::for_each(history_id_set.begin(), history_id_set.end(),
                                 AddHistoryMatch(*this,
                                                 lower_terms)).ScoredMatches();
  }

  // Remove any stale TermCharWordSet's.
  term_char_word_set_cache_.erase(
      std::remove_if(term_char_word_set_cache_.begin(),
                     term_char_word_set_cache_.end(),
                     std::mem_fun_ref(&TermCharWordSet::IsNotUsed)),
      term_char_word_set_cache_.end());
  return scored_items;
}

void InMemoryURLIndex::ResetTermCharWordSetCache() {
  // TODO(mrossetti): Consider keeping more of the cache around for possible
  // repeat searches, except a 'shortcuts' approach might be better for that.
  // TODO(mrossetti): Change TermCharWordSet to a class and use for_each.
  for (TermCharWordSetVector::iterator iter =
       term_char_word_set_cache_.begin();
       iter != term_char_word_set_cache_.end(); ++iter)
    iter->used_ = false;
}

InMemoryURLIndex::HistoryIDSet InMemoryURLIndex::HistoryIDSetFromWords(
    const string16& uni_string) {
  // Break the terms down into individual terms (words), get the candidate
  // set for each term, and intersect each to get a final candidate list.
  // Note that a single 'term' from the user's perspective might be
  // a string like "http://www.somewebsite.com" which, from our perspective,
  // is four words: 'http', 'www', 'somewebsite', and 'com'.
  HistoryIDSet history_id_set;
  String16Set words = WordSetFromString16(uni_string);
  bool first_word = true;
  for (String16Set::iterator iter = words.begin();
       iter != words.end(); ++iter) {
    String16Set::value_type uni_word = *iter;
    HistoryIDSet term_history_id_set =
        InMemoryURLIndex::HistoryIDsForTerm(uni_word);
    if (first_word) {
      history_id_set.swap(term_history_id_set);
      first_word = false;
    } else {
      HistoryIDSet old_history_id_set(history_id_set);
      history_id_set.clear();
      std::set_intersection(old_history_id_set.begin(),
                            old_history_id_set.end(),
                            term_history_id_set.begin(),
                            term_history_id_set.end(),
                            std::inserter(history_id_set,
                                          history_id_set.begin()));
    }
  }
  return history_id_set;
}

InMemoryURLIndex::HistoryIDSet InMemoryURLIndex::HistoryIDsForTerm(
    const string16& uni_word) {
  InMemoryURLIndex::HistoryIDSet history_id_set;

  // For each character in the word, get the char/word_id map entry and
  // intersect with the set in an incremental manner.
  Char16Set uni_chars = CharactersFromString16(uni_word);
  WordIDSet word_id_set(WordIDSetForTermChars(uni_chars));

  // If any words resulted then we can compose a set of history IDs by unioning
  // the sets from each word.
  if (!word_id_set.empty()) {
    for (WordIDSet::iterator word_id_iter = word_id_set.begin();
         word_id_iter != word_id_set.end(); ++word_id_iter) {
      WordID word_id = *word_id_iter;
      WordIDHistoryMap::iterator word_iter = word_id_history_map_.find(word_id);
      if (word_iter != word_id_history_map_.end()) {
        HistoryIDSet& word_history_id_set(word_iter->second);
        history_id_set.insert(word_history_id_set.begin(),
                              word_history_id_set.end());
      }
    }
  }

  return history_id_set;
}

// Utility Functions

// static
InMemoryURLIndex::String16Set InMemoryURLIndex::WordSetFromString16(
    const string16& uni_string) {
  String16Set words;
  base::BreakIterator iter(&uni_string, base::BreakIterator::BREAK_WORD);
  if (iter.Init()) {
    while (iter.Advance()) {
      if (iter.IsWord())
        words.insert(iter.GetString());
    }
  }
  return words;
}

// static
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

InMemoryURLIndex::WordIDSet InMemoryURLIndex::WordIDSetForTermChars(
    InMemoryURLIndex::Char16Set const& uni_chars) {
  TermCharWordSet* best_term_char_word_set = NULL;
  bool set_found = false;
  size_t outstanding_count = 0;
  Char16Set outstanding_chars;
  for (TermCharWordSetVector::iterator iter = term_char_word_set_cache_.begin();
       iter != term_char_word_set_cache_.end(); ++iter) {
    TermCharWordSetVector::value_type& term_char_word_set(*iter);
    Char16Set& char_set(term_char_word_set.char_set_);
    Char16Set exclusions;
    std::set_difference(char_set.begin(), char_set.end(),
                        uni_chars.begin(), uni_chars.end(),
                        std::inserter(exclusions, exclusions.begin()));
    if (exclusions.empty()) {
      // Do a reverse difference so that we know which characters remain
      // to be indexed. Then decide if this is a better match than any
      // previous cached set.
      std::set_difference(uni_chars.begin(), uni_chars.end(),
                          char_set.begin(), char_set.end(),
                          std::inserter(exclusions, exclusions.begin()));
      if (!set_found || exclusions.size() < outstanding_count) {
        outstanding_chars = exclusions;
        best_term_char_word_set = &*iter;
        outstanding_count = exclusions.size();
        set_found = true;
      }
    }
  }

  WordIDSet word_id_set;
  if (set_found && outstanding_count == 0) {
    // If there were no oustanding characters then we can use the cached one.
    best_term_char_word_set->used_ = true;
    word_id_set = best_term_char_word_set->word_id_set_;
  } else {
    // Some or all of the characters remain to be indexed.
    bool first_character = true;
    if (set_found) {
      first_character = false;
      word_id_set = best_term_char_word_set->word_id_set_;
    } else {
      outstanding_chars = uni_chars;
    }

    for (Char16Set::iterator uni_char_iter = outstanding_chars.begin();
         uni_char_iter != outstanding_chars.end(); ++uni_char_iter) {
      Char16Set::value_type uni_char = *uni_char_iter;
      CharWordIDMap::iterator char_iter = char_word_map_.find(uni_char);
      if (char_iter == char_word_map_.end()) {
        // The character was not found so bail.
        word_id_set.clear();
        break;
      }
      WordIDSet& char_word_id_set(char_iter->second);
      if (first_character) {
        word_id_set = char_word_id_set;
        first_character = false;
      } else {
        WordIDSet old_word_id_set(word_id_set);
        word_id_set.clear();
        std::set_intersection(old_word_id_set.begin(),
                              old_word_id_set.end(),
                              char_word_id_set.begin(),
                              char_word_id_set.end(),
                              std::inserter(word_id_set,
                              word_id_set.begin()));
      }
    }
    // Update the cache.
    if (!set_found || outstanding_count) {
      term_char_word_set_cache_.push_back(TermCharWordSet(uni_chars,
          word_id_set, true));
    }
  }
  return word_id_set;
}

// static
int InMemoryURLIndex::RawScoreForURL(const URLRow& row,
                                     const String16Vector& terms,
                                     size_t* first_term_location) {
  GURL gurl = row.url();
  if (!gurl.is_valid())
    return 0;

  string16 url = UTF8ToUTF16(gurl.spec());

  // Collect all term start positions so we can see if they appear in order.
  std::vector<size_t> term_locations;
  int out_of_order = 0;  // Count the terms which are out of order.
  size_t start_location_total = 0;
  size_t term_length_total = 0;
  for (String16Vector::const_iterator iter = terms.begin(); iter != terms.end();
       ++iter) {
    string16 term = *iter;
    size_t term_location = url.find(term);
    if (term_location == string16::npos)
      return 0;  // A term was not found.  This should not happen.
    if (iter != terms.begin()) {
      // See if this term is out-of-order.
      for (std::vector<size_t>::const_iterator order_iter =
               term_locations.begin(); order_iter != term_locations.end();
               ++order_iter) {
        if (term_location <= *order_iter)
          ++out_of_order;
      }
    } else {
      *first_term_location = term_location;
    }
    term_locations.push_back(term_location);
    start_location_total += term_location;
    term_length_total += term.size();
  }

  // Calculate a raw score.
  // TODO(mrossetti): This is good enough for now but must be fine-tuned.
  const float kOrderMaxValue = 10.0;
  float order_value = 10.0;
  if (terms.size() > 1) {
    int max_possible_out_of_order = (terms.size() * (terms.size() - 1)) / 2;
    order_value =
        (static_cast<float>(max_possible_out_of_order - out_of_order) /
         max_possible_out_of_order) * kOrderMaxValue;
  }

  const float kStartMaxValue = 10.0;
  const size_t kMaxSignificantStart = 20;
  float start_value =
      (static_cast<float>(kMaxSignificantStart -
       std::min(kMaxSignificantStart, start_location_total / terms.size()))) /
      static_cast<float>(kMaxSignificantStart) * kStartMaxValue;

  const float kCompleteMaxValue = 10.0;
  float complete_value =
      (static_cast<float>(term_length_total) / static_cast<float>(url.size())) *
      kStartMaxValue;

  const float kLastVisitMaxValue = 10.0;
  const base::TimeDelta kMaxSignificantDay = base::TimeDelta::FromDays(30);
  int64 delta_time = (kMaxSignificantDay -
      std::min((base::Time::Now() - row.last_visit()),
               kMaxSignificantDay)).ToInternalValue();
  float last_visit_value =
      (static_cast<float>(delta_time) /
       static_cast<float>(kMaxSignificantDay.ToInternalValue())) *
      kLastVisitMaxValue;

  const float kVisitCountMaxValue = 10.0;
  const int kMaxSignificantVisits = 10;
  float visit_count_value =
      (static_cast<float>(std::min(row.visit_count(),
       kMaxSignificantVisits))) / static_cast<float>(kMaxSignificantVisits) *
      kVisitCountMaxValue;

  const float kTypedCountMaxValue = 20.0;
  const int kMaxSignificantTyped = 10;
  float typed_count_value =
      (static_cast<float>(std::min(row.typed_count(),
       kMaxSignificantTyped))) / static_cast<float>(kMaxSignificantTyped) *
      kTypedCountMaxValue;

  float raw_score = order_value + start_value + complete_value +
      last_visit_value + visit_count_value + typed_count_value;

  // Normalize the score.
  const float kMaxNormalizedRawScore = 1000.0;
  raw_score =
      (raw_score / (kOrderMaxValue + kStartMaxValue + kCompleteMaxValue +
                    kLastVisitMaxValue + kVisitCountMaxValue +
                    kTypedCountMaxValue)) *
      kMaxNormalizedRawScore;
  return static_cast<int>(raw_score);
}

// static
Time InMemoryURLIndex::RecentThreshold() {
  return Time::Now() - TimeDelta::FromDays(kLowQualityMatchAgeLimitInDays);
}

InMemoryURLIndex::AddHistoryMatch::AddHistoryMatch(
    const InMemoryURLIndex& index,
    const String16Vector& lower_terms)
    : index_(index),
      lower_terms_(lower_terms) {
}

InMemoryURLIndex::AddHistoryMatch::~AddHistoryMatch() {}

void InMemoryURLIndex::AddHistoryMatch::operator()(
    const InMemoryURLIndex::HistoryID history_id) {
  HistoryInfoMap::const_iterator hist_pos =
      index_.history_info_map_.find(history_id);
  if (hist_pos != index_.history_info_map_.end()) {
    const URLRow& hist_item = hist_pos->second;
    // TODO(mrossetti): Accommodate multiple term highlighting.
    size_t input_location = 0;
    int score = InMemoryURLIndex::RawScoreForURL(
        hist_item, lower_terms_, &input_location);
    if (score != 0) {
      // We only retain the top 10 highest scoring results so
      // see if this one fits into the top 10 and, if so, where.
      ScoredHistoryMatches::iterator scored_iter = scored_matches_.begin();
      while (scored_iter != scored_matches_.end() &&
             (*scored_iter).raw_score > score)
        ++scored_iter;
      if ((scored_matches_.size() < 10) ||
          (scored_iter != scored_matches_.end())) {
        // Create and insert the new item.
        // TODO(mrossetti): Properly set |match_in_scheme| and
        // |innermost_match|.
        bool match_in_scheme = false;
        bool innermost_match = true;
        ScoredHistoryMatch match(hist_item, input_location,
            match_in_scheme, innermost_match, score);
        if (!scored_matches_.empty())
          scored_matches_.insert(scored_iter, match);
        else
          scored_matches_.push_back(match);
        // Trim any entries beyond 10.
        if (scored_matches_.size() > 10) {
          scored_matches_.erase(scored_matches_.begin() + 10,
                                scored_matches_.end());
        }
      }
    }
  }
}

}  // namespace history
