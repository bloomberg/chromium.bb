// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/url_index_private_data.h"

#include <functional>
#include <iterator>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_util.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/in_memory_url_index.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/history/core/browser/history_client.h"
#include "net/base/net_util.h"

#if defined(USE_SYSTEM_PROTOBUF)
#include <google/protobuf/repeated_field.h>
#else
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
#endif

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;
using in_memory_url_index::InMemoryURLIndexCacheItem;

namespace {
static const size_t kMaxVisitsToStoreInCache = 10u;
}  // anonymous namespace

namespace history {

typedef imui::InMemoryURLIndexCacheItem_WordListItem WordListItem;
typedef imui::InMemoryURLIndexCacheItem_WordMapItem_WordMapEntry WordMapEntry;
typedef imui::InMemoryURLIndexCacheItem_WordMapItem WordMapItem;
typedef imui::InMemoryURLIndexCacheItem_CharWordMapItem CharWordMapItem;
typedef imui::InMemoryURLIndexCacheItem_CharWordMapItem_CharWordMapEntry
    CharWordMapEntry;
typedef imui::InMemoryURLIndexCacheItem_WordIDHistoryMapItem
    WordIDHistoryMapItem;
typedef imui::
    InMemoryURLIndexCacheItem_WordIDHistoryMapItem_WordIDHistoryMapEntry
    WordIDHistoryMapEntry;
typedef imui::InMemoryURLIndexCacheItem_HistoryInfoMapItem HistoryInfoMapItem;
typedef imui::InMemoryURLIndexCacheItem_HistoryInfoMapItem_HistoryInfoMapEntry
    HistoryInfoMapEntry;
typedef imui::
    InMemoryURLIndexCacheItem_HistoryInfoMapItem_HistoryInfoMapEntry_VisitInfo
    HistoryInfoMapEntry_VisitInfo;
typedef imui::InMemoryURLIndexCacheItem_WordStartsMapItem WordStartsMapItem;
typedef imui::InMemoryURLIndexCacheItem_WordStartsMapItem_WordStartsMapEntry
    WordStartsMapEntry;


// Algorithm Functions ---------------------------------------------------------

// Comparison function for sorting search terms by descending length.
bool LengthGreater(const base::string16& string_a,
                   const base::string16& string_b) {
  return string_a.length() > string_b.length();
}


// UpdateRecentVisitsFromHistoryDBTask -----------------------------------------

// HistoryDBTask used to update the recent visit data for a particular
// row from the history database.
class UpdateRecentVisitsFromHistoryDBTask : public HistoryDBTask {
 public:
  explicit UpdateRecentVisitsFromHistoryDBTask(
      URLIndexPrivateData* private_data,
      URLID url_id);

  virtual bool RunOnDBThread(HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE;
  virtual void DoneRunOnMainThread() OVERRIDE;

 private:
  virtual ~UpdateRecentVisitsFromHistoryDBTask();

  // The URLIndexPrivateData that gets updated after the historyDB
  // task returns.
  URLIndexPrivateData* private_data_;
  // The ID of the URL to get visits for and then update.
  URLID url_id_;
  // Whether fetching the recent visits for the URL succeeded.
  bool succeeded_;
  // The awaited data that's shown to private_data_ for it to copy and
  // store.
  VisitVector recent_visits_;

  DISALLOW_COPY_AND_ASSIGN(UpdateRecentVisitsFromHistoryDBTask);
};

UpdateRecentVisitsFromHistoryDBTask::UpdateRecentVisitsFromHistoryDBTask(
    URLIndexPrivateData* private_data,
    URLID url_id)
    : private_data_(private_data),
      url_id_(url_id),
      succeeded_(false) {
}

bool UpdateRecentVisitsFromHistoryDBTask::RunOnDBThread(
    HistoryBackend* backend,
    HistoryDatabase* db) {
  // Make sure the private data is going to get as many recent visits as
  // ScoredHistoryMatch::GetFrequency() hopes to use.
  DCHECK_GE(kMaxVisitsToStoreInCache, ScoredHistoryMatch::kMaxVisitsToScore);
  succeeded_ = db->GetMostRecentVisitsForURL(url_id_,
                                             kMaxVisitsToStoreInCache,
                                             &recent_visits_);
  if (!succeeded_)
    recent_visits_.clear();
  return true;  // Always claim to be done; do not retry failures.
}

void UpdateRecentVisitsFromHistoryDBTask::DoneRunOnMainThread() {
  if (succeeded_)
    private_data_->UpdateRecentVisits(url_id_, recent_visits_);
}

UpdateRecentVisitsFromHistoryDBTask::~UpdateRecentVisitsFromHistoryDBTask() {
}


// URLIndexPrivateData ---------------------------------------------------------

URLIndexPrivateData::URLIndexPrivateData()
    : restored_cache_version_(0),
      saved_cache_version_(kCurrentCacheFileVersion),
      pre_filter_item_count_(0),
      post_filter_item_count_(0),
      post_scoring_item_count_(0) {
}

ScoredHistoryMatches URLIndexPrivateData::HistoryItemsForTerms(
    base::string16 search_string,
    size_t cursor_position,
    size_t max_matches,
    const std::string& languages,
    HistoryClient* history_client) {
  // If cursor position is set and useful (not at either end of the
  // string), allow the search string to be broken at cursor position.
  // We do this by pretending there's a space where the cursor is.
  if ((cursor_position != base::string16::npos) &&
      (cursor_position < search_string.length()) &&
      (cursor_position > 0)) {
    search_string.insert(cursor_position, base::ASCIIToUTF16(" "));
  }
  pre_filter_item_count_ = 0;
  post_filter_item_count_ = 0;
  post_scoring_item_count_ = 0;
  // The search string we receive may contain escaped characters. For reducing
  // the index we need individual, lower-cased words, ignoring escapings. For
  // the final filtering we need whitespace separated substrings possibly
  // containing escaped characters.
  base::string16 lower_raw_string(base::i18n::ToLower(search_string));
  base::string16 lower_unescaped_string =
      net::UnescapeURLComponent(lower_raw_string,
          net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS);
  // Extract individual 'words' (as opposed to 'terms'; see below) from the
  // search string. When the user types "colspec=ID%20Mstone Release" we get
  // four 'words': "colspec", "id", "mstone" and "release".
  String16Vector lower_words(
      history::String16VectorFromString16(lower_unescaped_string, false, NULL));
  ScoredHistoryMatches scored_items;

  // Do nothing if we have indexed no words (probably because we've not been
  // initialized yet) or the search string has no words.
  if (word_list_.empty() || lower_words.empty()) {
    search_term_cache_.clear();  // Invalidate the term cache.
    return scored_items;
  }

  // Reset used_ flags for search_term_cache_. We use a basic mark-and-sweep
  // approach.
  ResetSearchTermCache();

  HistoryIDSet history_id_set = HistoryIDSetFromWords(lower_words);

  // Trim the candidate pool if it is large. Note that we do not filter out
  // items that do not contain the search terms as proper substrings -- doing
  // so is the performance-costly operation we are trying to avoid in order
  // to maintain omnibox responsiveness.
  const size_t kItemsToScoreLimit = 500;
  pre_filter_item_count_ = history_id_set.size();
  // If we trim the results set we do not want to cache the results for next
  // time as the user's ultimately desired result could easily be eliminated
  // in this early rough filter.
  bool was_trimmed = (pre_filter_item_count_ > kItemsToScoreLimit);
  if (was_trimmed) {
    HistoryIDVector history_ids;
    std::copy(history_id_set.begin(), history_id_set.end(),
              std::back_inserter(history_ids));
    // Trim down the set by sorting by typed-count, visit-count, and last
    // visit.
    HistoryItemFactorGreater
        item_factor_functor(history_info_map_);
    std::partial_sort(history_ids.begin(),
                      history_ids.begin() + kItemsToScoreLimit,
                      history_ids.end(),
                      item_factor_functor);
    history_id_set.clear();
    std::copy(history_ids.begin(), history_ids.begin() + kItemsToScoreLimit,
              std::inserter(history_id_set, history_id_set.end()));
    post_filter_item_count_ = history_id_set.size();
  }

  // Pass over all of the candidates filtering out any without a proper
  // substring match, inserting those which pass in order by score. Note that
  // in this step we are using the raw search string complete with escaped
  // URL elements. When the user has specifically typed something akin to
  // "sort=pri&colspec=ID%20Mstone%20Release" we want to make sure that that
  // specific substring appears in the URL or page title.

  // We call these 'terms' (as opposed to 'words'; see above) as in this case
  // we only want to break up the search string on 'true' whitespace rather than
  // escaped whitespace. When the user types "colspec=ID%20Mstone Release" we
  // get two 'terms': "colspec=id%20mstone" and "release".
  history::String16Vector lower_raw_terms;
  if (Tokenize(lower_raw_string, base::kWhitespaceUTF16,
               &lower_raw_terms) == 0) {
    // Don't score matches when there are no terms to score against.  (It's
    // possible that the word break iterater that extracts words to search
    // for in the database allows some whitespace "words" whereas Tokenize
    // excludes a long list of whitespace.)  One could write a scoring
    // function that gives a reasonable order to matches when there
    // are no terms (i.e., all the words are some form of whitespace),
    // but this is such a rare edge case that it's not worth the time.
    return scored_items;
  }
  scored_items = std::for_each(history_id_set.begin(), history_id_set.end(),
      AddHistoryMatch(*this, languages, history_client, lower_raw_string,
                      lower_raw_terms, base::Time::Now())).ScoredMatches();

  // Select and sort only the top |max_matches| results.
  if (scored_items.size() > max_matches) {
    std::partial_sort(scored_items.begin(),
                      scored_items.begin() +
                          max_matches,
                      scored_items.end(),
                      ScoredHistoryMatch::MatchScoreGreater);
      scored_items.resize(max_matches);
  } else {
    std::sort(scored_items.begin(), scored_items.end(),
              ScoredHistoryMatch::MatchScoreGreater);
  }
  post_scoring_item_count_ = scored_items.size();

  if (was_trimmed) {
    search_term_cache_.clear();  // Invalidate the term cache.
  } else {
    // Remove any stale SearchTermCacheItems.
    for (SearchTermCacheMap::iterator cache_iter = search_term_cache_.begin();
         cache_iter != search_term_cache_.end(); ) {
      if (!cache_iter->second.used_)
        search_term_cache_.erase(cache_iter++);
      else
        ++cache_iter;
    }
  }

  return scored_items;
}

bool URLIndexPrivateData::UpdateURL(
    HistoryService* history_service,
    const URLRow& row,
    const std::string& languages,
    const std::set<std::string>& scheme_whitelist,
    base::CancelableTaskTracker* tracker) {
  // The row may or may not already be in our index. If it is not already
  // indexed and it qualifies then it gets indexed. If it is already
  // indexed and still qualifies then it gets updated, otherwise it
  // is deleted from the index.
  bool row_was_updated = false;
  URLID row_id = row.id();
  HistoryInfoMap::iterator row_pos = history_info_map_.find(row_id);
  if (row_pos == history_info_map_.end()) {
    // This new row should be indexed if it qualifies.
    URLRow new_row(row);
    new_row.set_id(row_id);
    row_was_updated = RowQualifiesAsSignificant(new_row, base::Time()) &&
                      IndexRow(NULL,
                               history_service,
                               new_row,
                               languages,
                               scheme_whitelist,
                               tracker);
  } else if (RowQualifiesAsSignificant(row, base::Time())) {
    // This indexed row still qualifies and will be re-indexed.
    // The url won't have changed but the title, visit count, etc.
    // might have changed.
    URLRow& row_to_update = row_pos->second.url_row;
    bool title_updated = row_to_update.title() != row.title();
    if (row_to_update.visit_count() != row.visit_count() ||
        row_to_update.typed_count() != row.typed_count() ||
        row_to_update.last_visit() != row.last_visit() || title_updated) {
      row_to_update.set_visit_count(row.visit_count());
      row_to_update.set_typed_count(row.typed_count());
      row_to_update.set_last_visit(row.last_visit());
      // If something appears to have changed, update the recent visits
      // information.
      ScheduleUpdateRecentVisits(history_service, row_id, tracker);
      // While the URL is guaranteed to remain stable, the title may have
      // changed. If so, then update the index with the changed words.
      if (title_updated) {
        // Clear all words associated with this row and re-index both the
        // URL and title.
        RemoveRowWordsFromIndex(row_to_update);
        row_to_update.set_title(row.title());
        RowWordStarts word_starts;
        AddRowWordsToIndex(row_to_update, &word_starts, languages);
        word_starts_map_[row_id] = word_starts;
      }
      row_was_updated = true;
    }
  } else {
    // This indexed row no longer qualifies and will be de-indexed by
    // clearing all words associated with this row.
    RemoveRowFromIndex(row);
    row_was_updated = true;
  }
  if (row_was_updated)
    search_term_cache_.clear();  // This invalidates the cache.
  return row_was_updated;
}

void URLIndexPrivateData::UpdateRecentVisits(
    URLID url_id,
    const VisitVector& recent_visits) {
  HistoryInfoMap::iterator row_pos = history_info_map_.find(url_id);
  if (row_pos != history_info_map_.end()) {
    VisitInfoVector* visits = &row_pos->second.visits;
    visits->clear();
    const size_t size =
        std::min(recent_visits.size(), kMaxVisitsToStoreInCache);
    visits->reserve(size);
    for (size_t i = 0; i < size; i++) {
      // Copy from the VisitVector the only fields visits needs.
      visits->push_back(std::make_pair(recent_visits[i].visit_time,
                                       recent_visits[i].transition));
    }
  }
  // Else: Oddly, the URL doesn't seem to exist in the private index.
  // Ignore this update.  This can happen if, for instance, the user
  // removes the URL from URLIndexPrivateData before the historyDB call
  // returns.
}

void URLIndexPrivateData::ScheduleUpdateRecentVisits(
    HistoryService* history_service,
    URLID url_id,
    base::CancelableTaskTracker* tracker) {
  history_service->ScheduleDBTask(
      scoped_ptr<history::HistoryDBTask>(
          new UpdateRecentVisitsFromHistoryDBTask(this, url_id)), tracker);
}

// Helper functor for DeleteURL.
class HistoryInfoMapItemHasURL {
 public:
  explicit HistoryInfoMapItemHasURL(const GURL& url): url_(url) {}

  bool operator()(const std::pair<const HistoryID, HistoryInfoMapValue>& item) {
    return item.second.url_row.url() == url_;
  }

 private:
  const GURL& url_;
};

bool URLIndexPrivateData::DeleteURL(const GURL& url) {
  // Find the matching entry in the history_info_map_.
  HistoryInfoMap::iterator pos = std::find_if(
      history_info_map_.begin(),
      history_info_map_.end(),
      HistoryInfoMapItemHasURL(url));
  if (pos == history_info_map_.end())
    return false;
  RemoveRowFromIndex(pos->second.url_row);
  search_term_cache_.clear();  // This invalidates the cache.
  return true;
}

// static
scoped_refptr<URLIndexPrivateData> URLIndexPrivateData::RestoreFromFile(
    const base::FilePath& file_path,
    const std::string& languages) {
  base::TimeTicks beginning_time = base::TimeTicks::Now();
  if (!base::PathExists(file_path))
    return NULL;
  std::string data;
  // If there is no cache file then simply give up. This will cause us to
  // attempt to rebuild from the history database.
  if (!base::ReadFileToString(file_path, &data))
    return NULL;

  scoped_refptr<URLIndexPrivateData> restored_data(new URLIndexPrivateData);
  InMemoryURLIndexCacheItem index_cache;
  if (!index_cache.ParseFromArray(data.c_str(), data.size())) {
    LOG(WARNING) << "Failed to parse URLIndexPrivateData cache data read from "
                 << file_path.value();
    return restored_data;
  }

  if (!restored_data->RestorePrivateData(index_cache, languages))
    return NULL;

  UMA_HISTOGRAM_TIMES("History.InMemoryURLIndexRestoreCacheTime",
                      base::TimeTicks::Now() - beginning_time);
  UMA_HISTOGRAM_COUNTS("History.InMemoryURLHistoryItems",
                       restored_data->history_id_word_map_.size());
  UMA_HISTOGRAM_COUNTS("History.InMemoryURLCacheSize", data.size());
  UMA_HISTOGRAM_COUNTS_10000("History.InMemoryURLWords",
                             restored_data->word_map_.size());
  UMA_HISTOGRAM_COUNTS_10000("History.InMemoryURLChars",
                             restored_data->char_word_map_.size());
  if (restored_data->Empty())
    return NULL;  // 'No data' is the same as a failed reload.
  return restored_data;
}

// static
scoped_refptr<URLIndexPrivateData> URLIndexPrivateData::RebuildFromHistory(
    HistoryDatabase* history_db,
    const std::string& languages,
    const std::set<std::string>& scheme_whitelist) {
  if (!history_db)
    return NULL;

  base::TimeTicks beginning_time = base::TimeTicks::Now();

  scoped_refptr<URLIndexPrivateData>
      rebuilt_data(new URLIndexPrivateData);
  URLDatabase::URLEnumerator history_enum;
  if (!history_db->InitURLEnumeratorForSignificant(&history_enum))
    return NULL;
  rebuilt_data->last_time_rebuilt_from_history_ = base::Time::Now();
  for (URLRow row; history_enum.GetNextURL(&row); ) {
    rebuilt_data->IndexRow(
        history_db, NULL, row, languages, scheme_whitelist, NULL);
  }

  UMA_HISTOGRAM_TIMES("History.InMemoryURLIndexingTime",
                      base::TimeTicks::Now() - beginning_time);
  UMA_HISTOGRAM_COUNTS("History.InMemoryURLHistoryItems",
                       rebuilt_data->history_id_word_map_.size());
  UMA_HISTOGRAM_COUNTS_10000("History.InMemoryURLWords",
                             rebuilt_data->word_map_.size());
  UMA_HISTOGRAM_COUNTS_10000("History.InMemoryURLChars",
                             rebuilt_data->char_word_map_.size());
  return rebuilt_data;
}

// static
bool URLIndexPrivateData::WritePrivateDataToCacheFileTask(
    scoped_refptr<URLIndexPrivateData> private_data,
    const base::FilePath& file_path) {
  DCHECK(private_data.get());
  DCHECK(!file_path.empty());
  return private_data->SaveToFile(file_path);
}

scoped_refptr<URLIndexPrivateData> URLIndexPrivateData::Duplicate() const {
  scoped_refptr<URLIndexPrivateData> data_copy = new URLIndexPrivateData;
  data_copy->last_time_rebuilt_from_history_ = last_time_rebuilt_from_history_;
  data_copy->word_list_ = word_list_;
  data_copy->available_words_ = available_words_;
  data_copy->word_map_ = word_map_;
  data_copy->char_word_map_ = char_word_map_;
  data_copy->word_id_history_map_ = word_id_history_map_;
  data_copy->history_id_word_map_ = history_id_word_map_;
  data_copy->history_info_map_ = history_info_map_;
  data_copy->word_starts_map_ = word_starts_map_;
  return data_copy;
  // Not copied:
  //    search_term_cache_
  //    pre_filter_item_count_
  //    post_filter_item_count_
  //    post_scoring_item_count_
}

bool URLIndexPrivateData::Empty() const {
  return history_info_map_.empty();
}

void URLIndexPrivateData::Clear() {
  last_time_rebuilt_from_history_ = base::Time();
  word_list_.clear();
  available_words_.clear();
  word_map_.clear();
  char_word_map_.clear();
  word_id_history_map_.clear();
  history_id_word_map_.clear();
  history_info_map_.clear();
  word_starts_map_.clear();
}

URLIndexPrivateData::~URLIndexPrivateData() {}

HistoryIDSet URLIndexPrivateData::HistoryIDSetFromWords(
    const String16Vector& unsorted_words) {
  // Break the terms down into individual terms (words), get the candidate
  // set for each term, and intersect each to get a final candidate list.
  // Note that a single 'term' from the user's perspective might be
  // a string like "http://www.somewebsite.com" which, from our perspective,
  // is four words: 'http', 'www', 'somewebsite', and 'com'.
  HistoryIDSet history_id_set;
  String16Vector words(unsorted_words);
  // Sort the words into the longest first as such are likely to narrow down
  // the results quicker. Also, single character words are the most expensive
  // to process so save them for last.
  std::sort(words.begin(), words.end(), LengthGreater);
  for (String16Vector::iterator iter = words.begin(); iter != words.end();
       ++iter) {
    base::string16 uni_word = *iter;
    HistoryIDSet term_history_set = HistoryIDsForTerm(uni_word);
    if (term_history_set.empty()) {
      history_id_set.clear();
      break;
    }
    if (iter == words.begin()) {
      history_id_set.swap(term_history_set);
    } else {
      HistoryIDSet new_history_id_set = base::STLSetIntersection<HistoryIDSet>(
          history_id_set, term_history_set);
      history_id_set.swap(new_history_id_set);
    }
  }
  return history_id_set;
}

HistoryIDSet URLIndexPrivateData::HistoryIDsForTerm(
    const base::string16& term) {
  if (term.empty())
    return HistoryIDSet();

  // TODO(mrossetti): Consider optimizing for very common terms such as
  // 'http[s]', 'www', 'com', etc. Or collect the top 100 more frequently
  // occuring words in the user's searches.

  size_t term_length = term.length();
  WordIDSet word_id_set;
  if (term_length > 1) {
    // See if this term or a prefix thereof is present in the cache.
    SearchTermCacheMap::iterator best_prefix(search_term_cache_.end());
    for (SearchTermCacheMap::iterator cache_iter = search_term_cache_.begin();
         cache_iter != search_term_cache_.end(); ++cache_iter) {
      if (StartsWith(term, cache_iter->first, false) &&
          (best_prefix == search_term_cache_.end() ||
           cache_iter->first.length() > best_prefix->first.length()))
        best_prefix = cache_iter;
    }

    // If a prefix was found then determine the leftover characters to be used
    // for further refining the results from that prefix.
    Char16Set prefix_chars;
    base::string16 leftovers(term);
    if (best_prefix != search_term_cache_.end()) {
      // If the prefix is an exact match for the term then grab the cached
      // results and we're done.
      size_t prefix_length = best_prefix->first.length();
      if (prefix_length == term_length) {
        best_prefix->second.used_ = true;
        return best_prefix->second.history_id_set_;
      }

      // Otherwise we have a handy starting point.
      // If there are no history results for this prefix then we can bail early
      // as there will be no history results for the full term.
      if (best_prefix->second.history_id_set_.empty()) {
        search_term_cache_[term] = SearchTermCacheItem();
        return HistoryIDSet();
      }
      word_id_set = best_prefix->second.word_id_set_;
      prefix_chars = Char16SetFromString16(best_prefix->first);
      leftovers = term.substr(prefix_length);
    }

    // Filter for each remaining, unique character in the term.
    Char16Set leftover_chars = Char16SetFromString16(leftovers);
    Char16Set unique_chars =
        base::STLSetDifference<Char16Set>(leftover_chars, prefix_chars);

    // Reduce the word set with any leftover, unprocessed characters.
    if (!unique_chars.empty()) {
      WordIDSet leftover_set(WordIDSetForTermChars(unique_chars));
      // We might come up empty on the leftovers.
      if (leftover_set.empty()) {
        search_term_cache_[term] = SearchTermCacheItem();
        return HistoryIDSet();
      }
      // Or there may not have been a prefix from which to start.
      if (prefix_chars.empty()) {
        word_id_set.swap(leftover_set);
      } else {
        WordIDSet new_word_id_set = base::STLSetIntersection<WordIDSet>(
            word_id_set, leftover_set);
        word_id_set.swap(new_word_id_set);
      }
    }

    // We must filter the word list because the resulting word set surely
    // contains words which do not have the search term as a proper subset.
    for (WordIDSet::iterator word_set_iter = word_id_set.begin();
         word_set_iter != word_id_set.end(); ) {
      if (word_list_[*word_set_iter].find(term) == base::string16::npos)
        word_id_set.erase(word_set_iter++);
      else
        ++word_set_iter;
    }
  } else {
    word_id_set = WordIDSetForTermChars(Char16SetFromString16(term));
  }

  // If any words resulted then we can compose a set of history IDs by unioning
  // the sets from each word.
  HistoryIDSet history_id_set;
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

  // Record a new cache entry for this word if the term is longer than
  // a single character.
  if (term_length > 1)
    search_term_cache_[term] = SearchTermCacheItem(word_id_set, history_id_set);

  return history_id_set;
}

WordIDSet URLIndexPrivateData::WordIDSetForTermChars(
    const Char16Set& term_chars) {
  WordIDSet word_id_set;
  for (Char16Set::const_iterator c_iter = term_chars.begin();
       c_iter != term_chars.end(); ++c_iter) {
    CharWordIDMap::iterator char_iter = char_word_map_.find(*c_iter);
    if (char_iter == char_word_map_.end()) {
      // A character was not found so there are no matching results: bail.
      word_id_set.clear();
      break;
    }
    WordIDSet& char_word_id_set(char_iter->second);
    // It is possible for there to no longer be any words associated with
    // a particular character. Give up in that case.
    if (char_word_id_set.empty()) {
      word_id_set.clear();
      break;
    }

    if (c_iter == term_chars.begin()) {
      // First character results becomes base set of results.
      word_id_set = char_word_id_set;
    } else {
      // Subsequent character results get intersected in.
      WordIDSet new_word_id_set = base::STLSetIntersection<WordIDSet>(
          word_id_set, char_word_id_set);
      word_id_set.swap(new_word_id_set);
    }
  }
  return word_id_set;
}

bool URLIndexPrivateData::IndexRow(
    HistoryDatabase* history_db,
    HistoryService* history_service,
    const URLRow& row,
    const std::string& languages,
    const std::set<std::string>& scheme_whitelist,
    base::CancelableTaskTracker* tracker) {
  const GURL& gurl(row.url());

  // Index only URLs with a whitelisted scheme.
  if (!URLSchemeIsWhitelisted(gurl, scheme_whitelist))
    return false;

  URLID row_id = row.id();
  // Strip out username and password before saving and indexing.
  base::string16 url(net::FormatUrl(gurl, languages,
      net::kFormatUrlOmitUsernamePassword,
      net::UnescapeRule::NONE,
      NULL, NULL, NULL));

  HistoryID history_id = static_cast<HistoryID>(row_id);
  DCHECK_LT(history_id, std::numeric_limits<HistoryID>::max());

  // Add the row for quick lookup in the history info store.
  URLRow new_row(GURL(url), row_id);
  new_row.set_visit_count(row.visit_count());
  new_row.set_typed_count(row.typed_count());
  new_row.set_last_visit(row.last_visit());
  new_row.set_title(row.title());
  history_info_map_[history_id].url_row = new_row;

  // Index the words contained in the URL and title of the row.
  RowWordStarts word_starts;
  AddRowWordsToIndex(new_row, &word_starts, languages);
  word_starts_map_[history_id] = word_starts;

  // Update the recent visits information or schedule the update
  // as appropriate.
  if (history_db) {
    // We'd like to check that we're on the history DB thread.
    // However, unittest code actually calls this on the UI thread.
    // So we don't do any thread checks.
    VisitVector recent_visits;
    // Make sure the private data is going to get as many recent visits as
    // ScoredHistoryMatch::GetFrequency() hopes to use.
    DCHECK_GE(kMaxVisitsToStoreInCache, ScoredHistoryMatch::kMaxVisitsToScore);
    if (history_db->GetMostRecentVisitsForURL(row_id,
                                              kMaxVisitsToStoreInCache,
                                              &recent_visits))
      UpdateRecentVisits(row_id, recent_visits);
  } else {
    DCHECK(tracker);
    DCHECK(history_service);
    ScheduleUpdateRecentVisits(history_service, row_id, tracker);
  }

  return true;
}

void URLIndexPrivateData::AddRowWordsToIndex(const URLRow& row,
                                             RowWordStarts* word_starts,
                                             const std::string& languages) {
  HistoryID history_id = static_cast<HistoryID>(row.id());
  // Split URL into individual, unique words then add in the title words.
  const GURL& gurl(row.url());
  const base::string16& url =
      bookmarks::CleanUpUrlForMatching(gurl, languages, NULL);
  String16Set url_words = String16SetFromString16(url,
      word_starts ? &word_starts->url_word_starts_ : NULL);
  const base::string16& title = bookmarks::CleanUpTitleForMatching(row.title());
  String16Set title_words = String16SetFromString16(title,
      word_starts ? &word_starts->title_word_starts_ : NULL);
  String16Set words = base::STLSetUnion<String16Set>(url_words, title_words);
  for (String16Set::iterator word_iter = words.begin();
       word_iter != words.end(); ++word_iter)
    AddWordToIndex(*word_iter, history_id);

  search_term_cache_.clear();  // Invalidate the term cache.
}

void URLIndexPrivateData::AddWordToIndex(const base::string16& term,
                                         HistoryID history_id) {
  WordMap::iterator word_pos = word_map_.find(term);
  if (word_pos != word_map_.end())
    UpdateWordHistory(word_pos->second, history_id);
  else
    AddWordHistory(term, history_id);
}

void URLIndexPrivateData::AddWordHistory(const base::string16& term,
                                         HistoryID history_id) {
  WordID word_id = word_list_.size();
  if (available_words_.empty()) {
    word_list_.push_back(term);
  } else {
    word_id = *(available_words_.begin());
    word_list_[word_id] = term;
    available_words_.erase(word_id);
  }
  word_map_[term] = word_id;

  HistoryIDSet history_id_set;
  history_id_set.insert(history_id);
  word_id_history_map_[word_id] = history_id_set;
  AddToHistoryIDWordMap(history_id, word_id);

  // For each character in the newly added word (i.e. a word that is not
  // already in the word index), add the word to the character index.
  Char16Set characters = Char16SetFromString16(term);
  for (Char16Set::iterator uni_char_iter = characters.begin();
       uni_char_iter != characters.end(); ++uni_char_iter) {
    base::char16 uni_char = *uni_char_iter;
    CharWordIDMap::iterator char_iter = char_word_map_.find(uni_char);
    if (char_iter != char_word_map_.end()) {
      // Update existing entry in the char/word index.
      WordIDSet& word_id_set(char_iter->second);
      word_id_set.insert(word_id);
    } else {
      // Create a new entry in the char/word index.
      WordIDSet word_id_set;
      word_id_set.insert(word_id);
      char_word_map_[uni_char] = word_id_set;
    }
  }
}

void URLIndexPrivateData::UpdateWordHistory(WordID word_id,
                                            HistoryID history_id) {
  WordIDHistoryMap::iterator history_pos = word_id_history_map_.find(word_id);
  DCHECK(history_pos != word_id_history_map_.end());
  HistoryIDSet& history_id_set(history_pos->second);
  history_id_set.insert(history_id);
  AddToHistoryIDWordMap(history_id, word_id);
}

void URLIndexPrivateData::AddToHistoryIDWordMap(HistoryID history_id,
                                                WordID word_id) {
  HistoryIDWordMap::iterator iter = history_id_word_map_.find(history_id);
  if (iter != history_id_word_map_.end()) {
    WordIDSet& word_id_set(iter->second);
    word_id_set.insert(word_id);
  } else {
    WordIDSet word_id_set;
    word_id_set.insert(word_id);
    history_id_word_map_[history_id] = word_id_set;
  }
}

void URLIndexPrivateData::RemoveRowFromIndex(const URLRow& row) {
  RemoveRowWordsFromIndex(row);
  HistoryID history_id = static_cast<HistoryID>(row.id());
  history_info_map_.erase(history_id);
  word_starts_map_.erase(history_id);
}

void URLIndexPrivateData::RemoveRowWordsFromIndex(const URLRow& row) {
  // Remove the entries in history_id_word_map_ and word_id_history_map_ for
  // this row.
  HistoryID history_id = static_cast<HistoryID>(row.id());
  WordIDSet word_id_set = history_id_word_map_[history_id];
  history_id_word_map_.erase(history_id);

  // Reconcile any changes to word usage.
  for (WordIDSet::iterator word_id_iter = word_id_set.begin();
       word_id_iter != word_id_set.end(); ++word_id_iter) {
    WordID word_id = *word_id_iter;
    word_id_history_map_[word_id].erase(history_id);
    if (!word_id_history_map_[word_id].empty())
      continue;  // The word is still in use.

    // The word is no longer in use. Reconcile any changes to character usage.
    base::string16 word = word_list_[word_id];
    Char16Set characters = Char16SetFromString16(word);
    for (Char16Set::iterator uni_char_iter = characters.begin();
         uni_char_iter != characters.end(); ++uni_char_iter) {
      base::char16 uni_char = *uni_char_iter;
      char_word_map_[uni_char].erase(word_id);
      if (char_word_map_[uni_char].empty())
        char_word_map_.erase(uni_char);  // No longer in use.
    }

    // Complete the removal of references to the word.
    word_id_history_map_.erase(word_id);
    word_map_.erase(word);
    word_list_[word_id] = base::string16();
    available_words_.insert(word_id);
  }
}

void URLIndexPrivateData::ResetSearchTermCache() {
  for (SearchTermCacheMap::iterator iter = search_term_cache_.begin();
       iter != search_term_cache_.end(); ++iter)
    iter->second.used_ = false;
}

bool URLIndexPrivateData::SaveToFile(const base::FilePath& file_path) {
  base::TimeTicks beginning_time = base::TimeTicks::Now();
  InMemoryURLIndexCacheItem index_cache;
  SavePrivateData(&index_cache);
  std::string data;
  if (!index_cache.SerializeToString(&data)) {
    LOG(WARNING) << "Failed to serialize the InMemoryURLIndex cache.";
    return false;
  }

  int size = data.size();
  if (base::WriteFile(file_path, data.c_str(), size) != size) {
    LOG(WARNING) << "Failed to write " << file_path.value();
    return false;
  }
  UMA_HISTOGRAM_TIMES("History.InMemoryURLIndexSaveCacheTime",
                      base::TimeTicks::Now() - beginning_time);
  return true;
}

void URLIndexPrivateData::SavePrivateData(
    InMemoryURLIndexCacheItem* cache) const {
  DCHECK(cache);
  cache->set_last_rebuild_timestamp(
      last_time_rebuilt_from_history_.ToInternalValue());
  cache->set_version(saved_cache_version_);
  // history_item_count_ is no longer used but rather than change the protobuf
  // definition use a placeholder. This will go away with the switch to SQLite.
  cache->set_history_item_count(0);
  SaveWordList(cache);
  SaveWordMap(cache);
  SaveCharWordMap(cache);
  SaveWordIDHistoryMap(cache);
  SaveHistoryInfoMap(cache);
  SaveWordStartsMap(cache);
}

void URLIndexPrivateData::SaveWordList(InMemoryURLIndexCacheItem* cache) const {
  if (word_list_.empty())
    return;
  WordListItem* list_item = cache->mutable_word_list();
  list_item->set_word_count(word_list_.size());
  for (String16Vector::const_iterator iter = word_list_.begin();
       iter != word_list_.end(); ++iter)
    list_item->add_word(base::UTF16ToUTF8(*iter));
}

void URLIndexPrivateData::SaveWordMap(InMemoryURLIndexCacheItem* cache) const {
  if (word_map_.empty())
    return;
  WordMapItem* map_item = cache->mutable_word_map();
  map_item->set_item_count(word_map_.size());
  for (WordMap::const_iterator iter = word_map_.begin();
       iter != word_map_.end(); ++iter) {
    WordMapEntry* map_entry = map_item->add_word_map_entry();
    map_entry->set_word(base::UTF16ToUTF8(iter->first));
    map_entry->set_word_id(iter->second);
  }
}

void URLIndexPrivateData::SaveCharWordMap(
    InMemoryURLIndexCacheItem* cache) const {
  if (char_word_map_.empty())
    return;
  CharWordMapItem* map_item = cache->mutable_char_word_map();
  map_item->set_item_count(char_word_map_.size());
  for (CharWordIDMap::const_iterator iter = char_word_map_.begin();
       iter != char_word_map_.end(); ++iter) {
    CharWordMapEntry* map_entry = map_item->add_char_word_map_entry();
    map_entry->set_char_16(iter->first);
    const WordIDSet& word_id_set(iter->second);
    map_entry->set_item_count(word_id_set.size());
    for (WordIDSet::const_iterator set_iter = word_id_set.begin();
         set_iter != word_id_set.end(); ++set_iter)
      map_entry->add_word_id(*set_iter);
  }
}

void URLIndexPrivateData::SaveWordIDHistoryMap(
    InMemoryURLIndexCacheItem* cache) const {
  if (word_id_history_map_.empty())
    return;
  WordIDHistoryMapItem* map_item = cache->mutable_word_id_history_map();
  map_item->set_item_count(word_id_history_map_.size());
  for (WordIDHistoryMap::const_iterator iter = word_id_history_map_.begin();
       iter != word_id_history_map_.end(); ++iter) {
    WordIDHistoryMapEntry* map_entry =
        map_item->add_word_id_history_map_entry();
    map_entry->set_word_id(iter->first);
    const HistoryIDSet& history_id_set(iter->second);
    map_entry->set_item_count(history_id_set.size());
    for (HistoryIDSet::const_iterator set_iter = history_id_set.begin();
         set_iter != history_id_set.end(); ++set_iter)
      map_entry->add_history_id(*set_iter);
  }
}

void URLIndexPrivateData::SaveHistoryInfoMap(
    InMemoryURLIndexCacheItem* cache) const {
  if (history_info_map_.empty())
    return;
  HistoryInfoMapItem* map_item = cache->mutable_history_info_map();
  map_item->set_item_count(history_info_map_.size());
  for (HistoryInfoMap::const_iterator iter = history_info_map_.begin();
       iter != history_info_map_.end(); ++iter) {
    HistoryInfoMapEntry* map_entry = map_item->add_history_info_map_entry();
    map_entry->set_history_id(iter->first);
    const URLRow& url_row(iter->second.url_row);
    // Note: We only save information that contributes to the index so there
    // is no need to save search_term_cache_ (not persistent).
    map_entry->set_visit_count(url_row.visit_count());
    map_entry->set_typed_count(url_row.typed_count());
    map_entry->set_last_visit(url_row.last_visit().ToInternalValue());
    map_entry->set_url(url_row.url().spec());
    map_entry->set_title(base::UTF16ToUTF8(url_row.title()));
    const VisitInfoVector& visits(iter->second.visits);
    for (VisitInfoVector::const_iterator visit_iter = visits.begin();
         visit_iter != visits.end(); ++visit_iter) {
      HistoryInfoMapEntry_VisitInfo* visit_info = map_entry->add_visits();
      visit_info->set_visit_time(visit_iter->first.ToInternalValue());
      visit_info->set_transition_type(visit_iter->second);
    }
  }
}

void URLIndexPrivateData::SaveWordStartsMap(
    InMemoryURLIndexCacheItem* cache) const {
  if (word_starts_map_.empty())
    return;
  // For unit testing: Enable saving of the cache as an earlier version to
  // allow testing of cache file upgrading in ReadFromFile().
  // TODO(mrossetti): Instead of intruding on production code with this kind of
  // test harness, save a copy of an older version cache with known results.
  // Implement this when switching the caching over to SQLite.
  if (saved_cache_version_ < 1)
    return;

  WordStartsMapItem* map_item = cache->mutable_word_starts_map();
  map_item->set_item_count(word_starts_map_.size());
  for (WordStartsMap::const_iterator iter = word_starts_map_.begin();
       iter != word_starts_map_.end(); ++iter) {
    WordStartsMapEntry* map_entry = map_item->add_word_starts_map_entry();
    map_entry->set_history_id(iter->first);
    const RowWordStarts& word_starts(iter->second);
    for (WordStarts::const_iterator i = word_starts.url_word_starts_.begin();
         i != word_starts.url_word_starts_.end(); ++i)
      map_entry->add_url_word_starts(*i);
    for (WordStarts::const_iterator i = word_starts.title_word_starts_.begin();
         i != word_starts.title_word_starts_.end(); ++i)
      map_entry->add_title_word_starts(*i);
  }
}

bool URLIndexPrivateData::RestorePrivateData(
    const InMemoryURLIndexCacheItem& cache,
    const std::string& languages) {
  last_time_rebuilt_from_history_ =
      base::Time::FromInternalValue(cache.last_rebuild_timestamp());
  const base::TimeDelta rebuilt_ago =
      base::Time::Now() - last_time_rebuilt_from_history_;
  if ((rebuilt_ago > base::TimeDelta::FromDays(7)) ||
      (rebuilt_ago < base::TimeDelta::FromDays(-1))) {
    // Cache is more than a week old or, somehow, from some time in the future.
    // It's probably a good time to rebuild the index from history to
    // allow synced entries to now appear, expired entries to disappear, etc.
    // Allow one day in the future to make the cache not rebuild on simple
    // system clock changes such as time zone changes.
    return false;
  }
  if (cache.has_version()) {
    if (cache.version() < kCurrentCacheFileVersion) {
      // Don't try to restore an old format cache file.  (This will cause
      // the InMemoryURLIndex to schedule rebuilding the URLIndexPrivateData
      // from history.)
      return false;
    }
    restored_cache_version_ = cache.version();
  }
  return RestoreWordList(cache) && RestoreWordMap(cache) &&
      RestoreCharWordMap(cache) && RestoreWordIDHistoryMap(cache) &&
      RestoreHistoryInfoMap(cache) && RestoreWordStartsMap(cache, languages);
}

bool URLIndexPrivateData::RestoreWordList(
    const InMemoryURLIndexCacheItem& cache) {
  if (!cache.has_word_list())
    return false;
  const WordListItem& list_item(cache.word_list());
  uint32 expected_item_count = list_item.word_count();
  uint32 actual_item_count = list_item.word_size();
  if (actual_item_count == 0 || actual_item_count != expected_item_count)
    return false;
  const RepeatedPtrField<std::string>& words(list_item.word());
  for (RepeatedPtrField<std::string>::const_iterator iter = words.begin();
       iter != words.end(); ++iter)
    word_list_.push_back(base::UTF8ToUTF16(*iter));
  return true;
}

bool URLIndexPrivateData::RestoreWordMap(
    const InMemoryURLIndexCacheItem& cache) {
  if (!cache.has_word_map())
    return false;
  const WordMapItem& list_item(cache.word_map());
  uint32 expected_item_count = list_item.item_count();
  uint32 actual_item_count = list_item.word_map_entry_size();
  if (actual_item_count == 0 || actual_item_count != expected_item_count)
    return false;
  const RepeatedPtrField<WordMapEntry>& entries(list_item.word_map_entry());
  for (RepeatedPtrField<WordMapEntry>::const_iterator iter = entries.begin();
       iter != entries.end(); ++iter)
    word_map_[base::UTF8ToUTF16(iter->word())] = iter->word_id();
  return true;
}

bool URLIndexPrivateData::RestoreCharWordMap(
    const InMemoryURLIndexCacheItem& cache) {
  if (!cache.has_char_word_map())
    return false;
  const CharWordMapItem& list_item(cache.char_word_map());
  uint32 expected_item_count = list_item.item_count();
  uint32 actual_item_count = list_item.char_word_map_entry_size();
  if (actual_item_count == 0 || actual_item_count != expected_item_count)
    return false;
  const RepeatedPtrField<CharWordMapEntry>&
      entries(list_item.char_word_map_entry());
  for (RepeatedPtrField<CharWordMapEntry>::const_iterator iter =
       entries.begin(); iter != entries.end(); ++iter) {
    expected_item_count = iter->item_count();
    actual_item_count = iter->word_id_size();
    if (actual_item_count == 0 || actual_item_count != expected_item_count)
      return false;
    base::char16 uni_char = static_cast<base::char16>(iter->char_16());
    WordIDSet word_id_set;
    const RepeatedField<int32>& word_ids(iter->word_id());
    for (RepeatedField<int32>::const_iterator jiter = word_ids.begin();
         jiter != word_ids.end(); ++jiter)
      word_id_set.insert(*jiter);
    char_word_map_[uni_char] = word_id_set;
  }
  return true;
}

bool URLIndexPrivateData::RestoreWordIDHistoryMap(
    const InMemoryURLIndexCacheItem& cache) {
  if (!cache.has_word_id_history_map())
    return false;
  const WordIDHistoryMapItem& list_item(cache.word_id_history_map());
  uint32 expected_item_count = list_item.item_count();
  uint32 actual_item_count = list_item.word_id_history_map_entry_size();
  if (actual_item_count == 0 || actual_item_count != expected_item_count)
    return false;
  const RepeatedPtrField<WordIDHistoryMapEntry>&
      entries(list_item.word_id_history_map_entry());
  for (RepeatedPtrField<WordIDHistoryMapEntry>::const_iterator iter =
       entries.begin(); iter != entries.end(); ++iter) {
    expected_item_count = iter->item_count();
    actual_item_count = iter->history_id_size();
    if (actual_item_count == 0 || actual_item_count != expected_item_count)
      return false;
    WordID word_id = iter->word_id();
    HistoryIDSet history_id_set;
    const RepeatedField<int64>& history_ids(iter->history_id());
    for (RepeatedField<int64>::const_iterator jiter = history_ids.begin();
         jiter != history_ids.end(); ++jiter) {
      history_id_set.insert(*jiter);
      AddToHistoryIDWordMap(*jiter, word_id);
    }
    word_id_history_map_[word_id] = history_id_set;
  }
  return true;
}

bool URLIndexPrivateData::RestoreHistoryInfoMap(
    const InMemoryURLIndexCacheItem& cache) {
  if (!cache.has_history_info_map())
    return false;
  const HistoryInfoMapItem& list_item(cache.history_info_map());
  uint32 expected_item_count = list_item.item_count();
  uint32 actual_item_count = list_item.history_info_map_entry_size();
  if (actual_item_count == 0 || actual_item_count != expected_item_count)
    return false;
  const RepeatedPtrField<HistoryInfoMapEntry>&
      entries(list_item.history_info_map_entry());
  for (RepeatedPtrField<HistoryInfoMapEntry>::const_iterator iter =
       entries.begin(); iter != entries.end(); ++iter) {
    HistoryID history_id = iter->history_id();
    GURL url(iter->url());
    URLRow url_row(url, history_id);
    url_row.set_visit_count(iter->visit_count());
    url_row.set_typed_count(iter->typed_count());
    url_row.set_last_visit(base::Time::FromInternalValue(iter->last_visit()));
    if (iter->has_title()) {
      base::string16 title(base::UTF8ToUTF16(iter->title()));
      url_row.set_title(title);
    }
    history_info_map_[history_id].url_row = url_row;

    // Restore visits list.
    VisitInfoVector visits;
    visits.reserve(iter->visits_size());
    for (int i = 0; i < iter->visits_size(); ++i) {
      visits.push_back(std::make_pair(
          base::Time::FromInternalValue(iter->visits(i).visit_time()),
          ui::PageTransitionFromInt(iter->visits(i).transition_type())));
    }
    history_info_map_[history_id].visits = visits;
  }
  return true;
}

bool URLIndexPrivateData::RestoreWordStartsMap(
    const InMemoryURLIndexCacheItem& cache,
    const std::string& languages) {
  // Note that this function must be called after RestoreHistoryInfoMap() has
  // been run as the word starts may have to be recalculated from the urls and
  // page titles.
  if (cache.has_word_starts_map()) {
    const WordStartsMapItem& list_item(cache.word_starts_map());
    uint32 expected_item_count = list_item.item_count();
    uint32 actual_item_count = list_item.word_starts_map_entry_size();
    if (actual_item_count == 0 || actual_item_count != expected_item_count)
      return false;
    const RepeatedPtrField<WordStartsMapEntry>&
        entries(list_item.word_starts_map_entry());
    for (RepeatedPtrField<WordStartsMapEntry>::const_iterator iter =
         entries.begin(); iter != entries.end(); ++iter) {
      HistoryID history_id = iter->history_id();
      RowWordStarts word_starts;
      // Restore the URL word starts.
      const RepeatedField<int32>& url_starts(iter->url_word_starts());
      for (RepeatedField<int32>::const_iterator jiter = url_starts.begin();
           jiter != url_starts.end(); ++jiter)
        word_starts.url_word_starts_.push_back(*jiter);
      // Restore the page title word starts.
      const RepeatedField<int32>& title_starts(iter->title_word_starts());
      for (RepeatedField<int32>::const_iterator jiter = title_starts.begin();
           jiter != title_starts.end(); ++jiter)
        word_starts.title_word_starts_.push_back(*jiter);
      word_starts_map_[history_id] = word_starts;
    }
  } else {
    // Since the cache did not contain any word starts we must rebuild then from
    // the URL and page titles.
    for (HistoryInfoMap::const_iterator iter = history_info_map_.begin();
         iter != history_info_map_.end(); ++iter) {
      RowWordStarts word_starts;
      const URLRow& row(iter->second.url_row);
      const base::string16& url =
          bookmarks::CleanUpUrlForMatching(row.url(), languages, NULL);
      String16VectorFromString16(url, false, &word_starts.url_word_starts_);
      const base::string16& title =
          bookmarks::CleanUpTitleForMatching(row.title());
      String16VectorFromString16(title, false, &word_starts.title_word_starts_);
      word_starts_map_[iter->first] = word_starts;
    }
  }
  return true;
}

// static
bool URLIndexPrivateData::URLSchemeIsWhitelisted(
    const GURL& gurl,
    const std::set<std::string>& whitelist) {
  return whitelist.find(gurl.scheme()) != whitelist.end();
}


// SearchTermCacheItem ---------------------------------------------------------

URLIndexPrivateData::SearchTermCacheItem::SearchTermCacheItem(
    const WordIDSet& word_id_set,
    const HistoryIDSet& history_id_set)
    : word_id_set_(word_id_set),
      history_id_set_(history_id_set),
      used_(true) {}

URLIndexPrivateData::SearchTermCacheItem::SearchTermCacheItem()
    : used_(true) {}

URLIndexPrivateData::SearchTermCacheItem::~SearchTermCacheItem() {}


// URLIndexPrivateData::AddHistoryMatch ----------------------------------------

URLIndexPrivateData::AddHistoryMatch::AddHistoryMatch(
    const URLIndexPrivateData& private_data,
    const std::string& languages,
    HistoryClient* history_client,
    const base::string16& lower_string,
    const String16Vector& lower_terms,
    const base::Time now)
    : private_data_(private_data),
      languages_(languages),
      history_client_(history_client),
      lower_string_(lower_string),
      lower_terms_(lower_terms),
      now_(now) {
  // Calculate offsets for each term.  For instance, the offset for
  // ".net" should be 1, indicating that the actual word-part of the term
  // starts at offset 1.
  lower_terms_to_word_starts_offsets_.resize(lower_terms_.size(), 0u);
  for (size_t i = 0; i < lower_terms_.size(); ++i) {
    base::i18n::BreakIterator iter(lower_terms_[i],
                                   base::i18n::BreakIterator::BREAK_WORD);
    // If the iterator doesn't work, assume an offset of 0.
    if (!iter.Init())
      continue;
    // Find the first word start.
    while (iter.Advance() && !iter.IsWord()) {}
    if (iter.IsWord())
      lower_terms_to_word_starts_offsets_[i] = iter.prev();
    // Else: the iterator didn't find a word break.  Assume an offset of 0.
  }
}

URLIndexPrivateData::AddHistoryMatch::~AddHistoryMatch() {}

void URLIndexPrivateData::AddHistoryMatch::operator()(
    const HistoryID history_id) {
  HistoryInfoMap::const_iterator hist_pos =
      private_data_.history_info_map_.find(history_id);
  if (hist_pos != private_data_.history_info_map_.end()) {
    const URLRow& hist_item = hist_pos->second.url_row;
    const VisitInfoVector& visits = hist_pos->second.visits;
    WordStartsMap::const_iterator starts_pos =
        private_data_.word_starts_map_.find(history_id);
    DCHECK(starts_pos != private_data_.word_starts_map_.end());
    ScoredHistoryMatch match(hist_item, visits, languages_, lower_string_,
                             lower_terms_, lower_terms_to_word_starts_offsets_,
                             starts_pos->second, now_, history_client_);
    if (match.raw_score() > 0)
      scored_matches_.push_back(match);
  }
}


// URLIndexPrivateData::HistoryItemFactorGreater -------------------------------

URLIndexPrivateData::HistoryItemFactorGreater::HistoryItemFactorGreater(
    const HistoryInfoMap& history_info_map)
    : history_info_map_(history_info_map) {
}

URLIndexPrivateData::HistoryItemFactorGreater::~HistoryItemFactorGreater() {}

bool URLIndexPrivateData::HistoryItemFactorGreater::operator()(
    const HistoryID h1,
    const HistoryID h2) {
  HistoryInfoMap::const_iterator entry1(history_info_map_.find(h1));
  if (entry1 == history_info_map_.end())
    return false;
  HistoryInfoMap::const_iterator entry2(history_info_map_.find(h2));
  if (entry2 == history_info_map_.end())
    return true;
  const URLRow& r1(entry1->second.url_row);
  const URLRow& r2(entry2->second.url_row);
  // First cut: typed count, visit count, recency.
  // TODO(mrossetti): This is too simplistic. Consider an approach which ranks
  // recently visited (within the last 12/24 hours) as highly important. Get
  // input from mpearson.
  if (r1.typed_count() != r2.typed_count())
    return (r1.typed_count() > r2.typed_count());
  if (r1.visit_count() != r2.visit_count())
    return (r1.visit_count() > r2.visit_count());
  return (r1.last_visit() > r2.last_visit());
}

}  // namespace history
