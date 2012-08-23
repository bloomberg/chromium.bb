// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/url_index_private_data.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <limits>
#include <numeric>
#include <vector>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/in_memory_url_cache_database.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "net/base/net_util.h"

using content::BrowserThread;

namespace history {

// Comparison function for sorting search terms by descending length.
bool LengthGreater(const string16& string_a, const string16& string_b) {
  return string_a.length() > string_b.length();
}

// Initializes a whitelist of URL schemes.
void InitializeSchemeWhitelist(std::set<std::string>* whitelist) {
  DCHECK(whitelist);
  if (!whitelist->empty())
    return;  // Nothing to do, already initialized.
  whitelist->insert(std::string(chrome::kAboutScheme));
  whitelist->insert(std::string(chrome::kChromeUIScheme));
  whitelist->insert(std::string(chrome::kFileScheme));
  whitelist->insert(std::string(chrome::kFtpScheme));
  whitelist->insert(std::string(chrome::kHttpScheme));
  whitelist->insert(std::string(chrome::kHttpsScheme));
  whitelist->insert(std::string(chrome::kMailToScheme));
}

// CacheTransaction ------------------------------------------------------------

// Simple automatic helper class encapsulating calls to BeginTransaction/
// CommitTransaction.
class CacheTransaction {
 public:
  explicit CacheTransaction(InMemoryURLCacheDatabase* db);
  ~CacheTransaction();

 private:
  InMemoryURLCacheDatabase* db_;

  DISALLOW_COPY_AND_ASSIGN(CacheTransaction);
};

CacheTransaction::CacheTransaction(InMemoryURLCacheDatabase* db)
    : db_(db) {
  if (db_)
    db_->BeginTransaction();
}

CacheTransaction::~CacheTransaction() {
  if (db_)
    db_->CommitTransaction();
}

// URLIndexPrivateData Public (but only to InMemoryURLIndex) Functions ---------

URLIndexPrivateData::URLIndexPrivateData(const FilePath& history_dir,
                                         const std::string& languages)
    : history_dir_(history_dir),
      languages_(languages),
      cache_enabled_(true),
      shutdown_(false),
      lock_(),
      pre_filter_item_count_(0),
      post_filter_item_count_(0),
      post_scoring_item_count_(0) {
  InitializeSchemeWhitelist(&scheme_whitelist_);
}

bool URLIndexPrivateData::Init(
    base::SequencedWorkerPool::SequenceToken sequence_token) {
  // Since init occurs on the DB thread, it is possible that the profile was
  // told to shutdown before this task had a chance to kick off.
  base::AutoLock lock(lock_);
  if (shutdown_)
    return false;
  cache_db_ = new InMemoryURLCacheDatabase();
  FilePath dir_path;
  set_cache_enabled(GetCacheDBPath(&dir_path) &&
                    cache_db_->Init(dir_path, sequence_token));
  return cache_enabled();
}

void URLIndexPrivateData::Reset() {
  Clear();
  if (cache_enabled())
    cache_db_->Reset();
}

bool URLIndexPrivateData::Empty() const {
  return history_info_map_.empty();
}

URLIndexPrivateData* URLIndexPrivateData::Snapshot() const {
  return new URLIndexPrivateData(*this);
}

void URLIndexPrivateData::Shutdown() {
  base::AutoLock lock(lock_);
  shutdown_ = true;
  if (cache_enabled())
    cache_db_->Shutdown();
}

// Helper predicates for ValidateConsistency.
template <typename Pair>
struct SelectFirst : std::unary_function<Pair, typename Pair::first_type> {
  const typename Pair::first_type& operator()(const Pair& x) const {
    return x.first;
  }
};

template <typename Pair>
struct SelectSecond : std::unary_function<Pair, typename Pair::second_type> {
  const typename Pair::second_type& operator()(const Pair& x) const {
    return x.second;
  }
};

template <typename SourcePair, typename Target>
struct TargetInserter : std::unary_function<SourcePair, void> {
  explicit TargetInserter(Target* target) : target_(target) {}

  void operator()(const SourcePair& source) {
    target_->insert(source.second.begin(), source.second.end());
  }

  Target* target_;
};

bool URLIndexPrivateData::ValidateConsistency() const {
  // Scope things so that a large data set doesn't unnecessarily accumulate and
  // hang around.
  {
    // Make a set of WordIDs from word_map_.
    WordIDSet word_id_set_a;
    std::transform(word_map_.begin(), word_map_.end(),
                   std::inserter(word_id_set_a, word_id_set_a.begin()),
                   SelectSecond<WordMap::value_type>());

    // Compare word_map_'s word set to the words from word_id_history_map_.
    {
      WordIDSet word_id_set_b;
      std::transform(word_id_history_map_.begin(), word_id_history_map_.end(),
                     std::inserter(word_id_set_b, word_id_set_b.begin()),
                     SelectFirst<WordIDHistoryMap::value_type>());
      WordIDSet leftovers;
      std::set_symmetric_difference(
          word_id_set_a.begin(), word_id_set_a.end(),
          word_id_set_b.begin(), word_id_set_b.end(),
          std::inserter(leftovers, leftovers.begin()));
      if (!leftovers.empty())
        return false;
    }

    // Compare word_map_'s word set to the words from history_id_word_map_.
    {
      WordIDSet word_id_set_b;
      std::for_each(history_id_word_map_.begin(), history_id_word_map_.end(),
                    TargetInserter<HistoryIDWordMap::value_type,
                                   WordIDSet>(&word_id_set_b));
      WordIDSet leftovers;
      std::set_symmetric_difference(
          word_id_set_a.begin(), word_id_set_a.end(),
          word_id_set_b.begin(), word_id_set_b.end(),
          std::inserter(leftovers, leftovers.begin()));
      if (!leftovers.empty())
        return false;
    }

    // Compare word_map_'s word set to the words from char_word_map_.
    {
      WordIDSet word_id_set_b;
      std::for_each(char_word_map_.begin(), char_word_map_.end(),
                    TargetInserter<CharWordIDMap::value_type,
                                   WordIDSet>(&word_id_set_b));
      WordIDSet leftovers;
      std::set_symmetric_difference(
          word_id_set_a.begin(), word_id_set_a.end(),
          word_id_set_b.begin(), word_id_set_b.end(),
          std::inserter(leftovers, leftovers.begin()));
      if (!leftovers.empty())
        return false;
    }

    // Intersect available_words_ with set of WordIDs (created above from
    // word_map_) and test for no common WordIDs.
    {
      WordIDSet leftovers;
      std::set_intersection(word_id_set_a.begin(), word_id_set_a.end(),
                            available_words_.begin(), available_words_.end(),
                            std::inserter(leftovers, leftovers.begin()));
      if (!leftovers.empty())
        return false;
    }
  }

  {
    // Make a set of HistoryIDs from history_info_map_.
    HistoryIDSet history_id_set_a;
    std::transform(history_info_map_.begin(), history_info_map_.end(),
                   std::inserter(history_id_set_a, history_id_set_a.begin()),
                   SelectFirst<HistoryInfoMap::value_type>());

    // Compare history_info_map_'s set to word_id_history_map_'s HistoryIDs.
    {
      HistoryIDSet history_id_set_b;
      std::transform(history_info_map_.begin(), history_info_map_.end(),
                     std::inserter(history_id_set_b, history_id_set_b.begin()),
                     SelectFirst<HistoryInfoMap::value_type>());
      HistoryIDSet leftovers;
      std::set_symmetric_difference(
          history_id_set_a.begin(), history_id_set_a.end(),
          history_id_set_b.begin(), history_id_set_b.end(),
          std::inserter(leftovers, leftovers.begin()));
      if (!leftovers.empty())
        return false;
    }

    // Compare history_info_map_'s set to word_id_history_map_'s HistoryIDs.
    {
      HistoryIDSet history_id_set_b;
      std::for_each(word_id_history_map_.begin(), word_id_history_map_.end(),
                    TargetInserter<WordIDHistoryMap::value_type,
                                   HistoryIDSet>(&history_id_set_b));
      HistoryIDSet leftovers;
      std::set_symmetric_difference(
          history_id_set_a.begin(), history_id_set_a.end(),
          history_id_set_b.begin(), history_id_set_b.end(),
          std::inserter(leftovers, leftovers.begin()));
      if (!leftovers.empty())
        return false;
    }

    // Compare history_info_map_'s set to word_starts_map_'s HistoryIDs.
    {
      HistoryIDSet history_id_set_b;
      std::transform(word_starts_map_.begin(), word_starts_map_.end(),
                     std::inserter(history_id_set_b, history_id_set_b.begin()),
                     SelectFirst<WordStartsMap::value_type>());
      HistoryIDSet leftovers;
      std::set_symmetric_difference(
          history_id_set_a.begin(), history_id_set_a.end(),
          history_id_set_b.begin(), history_id_set_b.end(),
          std::inserter(leftovers, leftovers.begin()));
      if (!leftovers.empty())
        return false;
    }
  }

  // Make sets of words from word_list_ and word_map_ and test for equality.
  {
    String16Set word_list_set;
    std::copy(word_list_.begin(), word_list_.end(),
              std::inserter(word_list_set, word_list_set.end()));
    String16Set word_map_set;
    std::transform(word_map_.begin(), word_map_.end(),
                   std::inserter(word_map_set, word_map_set.begin()),
                   SelectFirst<WordMap::value_type>());
    if (word_list_set != word_map_set)
      return false;
  }

  return true;
}

ScoredHistoryMatches URLIndexPrivateData::HistoryItemsForTerms(
    const string16& search_string) {
  pre_filter_item_count_ = 0;
  post_filter_item_count_ = 0;
  post_scoring_item_count_ = 0;
  // The search string we receive may contain escaped characters. For reducing
  // the index we need individual, lower-cased words, ignoring escapings. For
  // the final filtering we need whitespace separated substrings possibly
  // containing escaped characters.
  string16 lower_raw_string(base::i18n::ToLower(search_string));
  string16 lower_unescaped_string =
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
  Tokenize(lower_raw_string, kWhitespaceUTF16, &lower_raw_terms);
  scored_items = std::for_each(history_id_set.begin(), history_id_set.end(),
      AddHistoryMatch(*this, lower_raw_string,
                      lower_raw_terms, base::Time::Now())).ScoredMatches();

  // Select and sort only the top kMaxMatches results.
  if (scored_items.size() > AutocompleteProvider::kMaxMatches) {
    std::partial_sort(scored_items.begin(),
                      scored_items.begin() +
                          AutocompleteProvider::kMaxMatches,
                      scored_items.end(),
                      ScoredHistoryMatch::MatchScoreGreater);
      scored_items.resize(AutocompleteProvider::kMaxMatches);
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

bool URLIndexPrivateData::UpdateURL(const URLRow& row) {
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
                      IndexRow(new_row);
  } else if (RowQualifiesAsSignificant(row, base::Time())) {
    // This indexed row still qualifies and will be re-indexed.
    // The url won't have changed but the title, visit count, etc.
    // might have changed.
    URLRow& row_to_update = row_pos->second;
    bool title_updated = row_to_update.title() != row.title();
    if (row_to_update.visit_count() != row.visit_count() ||
        row_to_update.typed_count() != row.typed_count() ||
        row_to_update.last_visit() != row.last_visit() || title_updated) {
      CacheTransaction cache_transaction(cache_enabled() ?
                                            cache_db_.get() : NULL);
      row_to_update.set_visit_count(row.visit_count());
      row_to_update.set_typed_count(row.typed_count());
      row_to_update.set_last_visit(row.last_visit());
      row_to_update.set_hidden(row.hidden());
      // While the URL is guaranteed to remain stable, the title may have
      // changed. If so, then update the index with the changed words.
      if (title_updated) {
        // Clear all words associated with this row and re-index both the
        // URL and title.
        RemoveRowWordsFromIndex(row_to_update);
        row_to_update.set_title(row.title());
        RowWordStarts word_starts;
        AddRowWordsToIndex(row_to_update, &word_starts);
        word_starts_map_[row_id] = word_starts;
        // Replace all word_starts for the row.
        if (cache_enabled()) {
          cache_db_->RemoveWordStarts(row_id);
          cache_db_->AddRowWordStarts(row_id, word_starts);
        }
      }
      if (cache_enabled()) {
        cache_db_->RemoveHistoryIDFromURLs(row_id);
        cache_db_->AddHistoryToURLs(row_id, row);
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

// Helper functor for DeleteURL.
class HistoryInfoMapItemHasURL {
 public:
  explicit HistoryInfoMapItemHasURL(const GURL& url): url_(url) {}

  bool operator()(const std::pair<const HistoryID, URLRow>& item) {
    return item.second.url() == url_;
  }

 private:
  const GURL& url_;
};

bool URLIndexPrivateData::DeleteURL(const GURL& url) {
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Find the matching entry in the history_info_map_.
  HistoryInfoMap::iterator pos = std::find_if(
      history_info_map_.begin(),
      history_info_map_.end(),
      HistoryInfoMapItemHasURL(url));
  if (pos == history_info_map_.end())
    return false;
  RemoveRowFromIndex(pos->second);
  search_term_cache_.clear();  // This invalidates the cache.
  return true;
}

bool URLIndexPrivateData::RestoreFromCacheTask() {
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         !BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(cache_db_);
  if (IsShutdown())
    return false;
  Clear();
  if (RestoreFromCache(cache_db_))
    return true;
  // If there was no SQLite-based cache then there might be an old,
  // deprecated, protobuf-based cache file laying around. Get rid of it.
  DeleteOldVersionCacheFile();
  return false;
}

// static
scoped_refptr<URLIndexPrivateData> URLIndexPrivateData::RebuildFromHistory(
    HistoryDatabase* history_db,
    scoped_refptr<URLIndexPrivateData> old_data) {
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         !BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!history_db)
    return NULL;

  scoped_refptr<URLIndexPrivateData> rebuilt_data(
      new URLIndexPrivateData(*(old_data.get())));

  // NOTE: We disable the cache database until after the replacement private
  // data has been created and then we re-enable the database. Once the private
  // data has been swapped in the database is refreshed with the new data.
  URLDatabase::URLEnumerator history_enum;
  if (!history_db->InitURLEnumeratorForSignificant(&history_enum))
    return NULL;

  for (URLRow row; !old_data->IsShutdown() && history_enum.GetNextURL(&row); )
    rebuilt_data->IndexRow(row);

  if (old_data->IsShutdown()) {
    rebuilt_data.release();
  }

  return rebuilt_data;
}

void URLIndexPrivateData::RefreshCacheTask() {
  DCHECK(!BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         !BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lock(lock_);
  if (shutdown_ || !cache_enabled())
    return;
  // Should a refresh fail for any reason we consider the database to be
  // corrupt and unavailable; no further remedial action is possible.
  set_cache_enabled(cache_db_->Reset() && cache_db_->Refresh(*this));
}

// static
void URLIndexPrivateData::InitializeSchemeWhitelistForTesting(
    std::set<std::string>* whitelist) {
  InitializeSchemeWhitelist(whitelist);
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
    const string16& lower_string,
    const String16Vector& lower_terms,
    const base::Time now)
  : private_data_(private_data),
    lower_string_(lower_string),
    lower_terms_(lower_terms),
    now_(now) {}

URLIndexPrivateData::AddHistoryMatch::~AddHistoryMatch() {}

void URLIndexPrivateData::AddHistoryMatch::operator()(
    const HistoryID history_id) {
  HistoryInfoMap::const_iterator hist_pos =
      private_data_.history_info_map_.find(history_id);
  if (hist_pos != private_data_.history_info_map_.end()) {
    const URLRow& hist_item = hist_pos->second;
    WordStartsMap::const_iterator starts_pos =
        private_data_.word_starts_map_.find(history_id);
    DCHECK(starts_pos != private_data_.word_starts_map_.end());
    ScoredHistoryMatch match(hist_item, lower_string_, lower_terms_,
                             starts_pos->second, now_);
    if (match.raw_score > 0)
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
  const URLRow& r1(entry1->second);
  const URLRow& r2(entry2->second);
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

// URLIndexPrivateData Private Functions ---------------------------------------

URLIndexPrivateData::URLIndexPrivateData(const URLIndexPrivateData& old_data)
    : history_dir_(old_data.history_dir_),
      languages_(old_data.languages_),
      cache_enabled_(old_data.cache_enabled_),
      shutdown_(old_data.IsShutdown()),
      lock_(),
      scheme_whitelist_(old_data.scheme_whitelist_),
      pre_filter_item_count_(0),
      post_filter_item_count_(0),
      post_scoring_item_count_(0) {
  cache_db_ = old_data.cache_db_;
}

// Called only by unit tests.
URLIndexPrivateData::URLIndexPrivateData()
    : cache_enabled_(true),
      shutdown_(false),
      lock_(),
      pre_filter_item_count_(0),
      post_filter_item_count_(0),
      post_scoring_item_count_(0) {
  InitializeSchemeWhitelist(&scheme_whitelist_);
}

URLIndexPrivateData::~URLIndexPrivateData() {}

bool URLIndexPrivateData::IsShutdown() const {
  base::AutoLock lock(lock_);
  return shutdown_;
}

void URLIndexPrivateData::Clear() {
  word_list_.clear();
  available_words_.clear();
  word_map_.clear();
  char_word_map_.clear();
  word_id_history_map_.clear();
  history_id_word_map_.clear();
  history_info_map_.clear();
  word_starts_map_.clear();
}

// Cache Updating --------------------------------------------------------------

bool URLIndexPrivateData::IndexRow(const URLRow& row) {
  const GURL& gurl(row.url());

  // Index only URLs with a whitelisted scheme.
  if (!URLSchemeIsWhitelisted(gurl, scheme_whitelist_))
    return false;

  // Strip out username and password before saving and indexing.
  string16 url(net::FormatUrl(gurl, languages_,
      net::kFormatUrlOmitUsernamePassword,
      net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS,
      NULL, NULL, NULL));

  URLID row_id = row.id();
  HistoryID history_id = static_cast<HistoryID>(row_id);
  DCHECK(history_info_map_.find(history_id) == history_info_map_.end());
  DCHECK_LT(history_id, std::numeric_limits<HistoryID>::max());

  // Add the row for quick lookup in the history info store.
  URLRow new_row(GURL(url), row_id);
  new_row.set_visit_count(row.visit_count());
  new_row.set_typed_count(row.typed_count());
  new_row.set_last_visit(row.last_visit());
  new_row.set_title(row.title());

  CacheTransaction cache_transaction(cache_enabled() ? cache_db_.get() : NULL);
  history_info_map_[history_id] = new_row;
  if (cache_enabled())
    cache_db_->AddHistoryToURLs(history_id, row);

  // Index the words contained in the URL and title of the row.
  RowWordStarts word_starts;
  AddRowWordsToIndex(new_row, &word_starts);
  word_starts_map_[history_id] = word_starts;
  if (cache_enabled())
    cache_db_->AddRowWordStarts(history_id, word_starts);
  return true;
}

void URLIndexPrivateData::AddRowWordsToIndex(const URLRow& row,
                                             RowWordStarts* word_starts) {
  HistoryID history_id = static_cast<HistoryID>(row.id());
  // Split URL into individual, unique words then add in the title words.
  const GURL& gurl(row.url());
  string16 url(net::FormatUrl(gurl, languages_,
      net::kFormatUrlOmitUsernamePassword,
      net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS,
      NULL, NULL, NULL));
  url = base::i18n::ToLower(url);
  String16Set url_words = String16SetFromString16(url,
      word_starts ? &word_starts->url_word_starts_ : NULL);
  String16Set title_words = String16SetFromString16(row.title(),
      word_starts ? &word_starts->title_word_starts_ : NULL);
  String16Set words;
  std::set_union(url_words.begin(), url_words.end(),
                 title_words.begin(), title_words.end(),
                 std::insert_iterator<String16Set>(words, words.begin()));
  for (String16Set::iterator word_iter = words.begin();
       word_iter != words.end(); ++word_iter)
    AddWordToIndex(*word_iter, history_id);

  search_term_cache_.clear();  // Invalidate the term cache.
}

void URLIndexPrivateData::AddWordToIndex(const string16& term,
                                         HistoryID history_id) {
  WordMap::iterator word_pos = word_map_.find(term);
  if (word_pos != word_map_.end())
    UpdateWordHistory(word_pos->second, history_id);
  else
    AddWordHistory(term, history_id);
}

void URLIndexPrivateData::UpdateWordHistory(WordID word_id,
                                            HistoryID history_id) {
  WordIDHistoryMap::iterator history_pos = word_id_history_map_.find(word_id);
  DCHECK(history_pos != word_id_history_map_.end());
  // There is no need to record changes to the word_id_history_map_ in the cache
  // because it can be rebuilt from the history_id_word_map_'s table.
  HistoryIDSet& history_id_set(history_pos->second);
  history_id_set.insert(history_id);
  AddToHistoryIDWordMap(history_id, word_id);

  // Add word_id/history_id entry to the word_history table
  if (cache_enabled())
    cache_db_->AddHistoryToWordHistory(word_id, history_id);
}

void URLIndexPrivateData::AddWordHistory(const string16& term,
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

  // Add word_id/term entry to the words table;
  if (cache_enabled())
    cache_db_->AddWordToWords(word_id, term);

  HistoryIDSet history_id_set;
  history_id_set.insert(history_id);
  word_id_history_map_[word_id] = history_id_set;
  AddToHistoryIDWordMap(history_id, word_id);

  // Add word_id/history_id entry to the word_history table
  if (cache_enabled())
    cache_db_->AddHistoryToWordHistory(word_id, history_id);

  // For each character in the newly added word (i.e. a word that is not
  // already in the word index), add the word to the character index.
  Char16Set characters = Char16SetFromString16(term);
  for (Char16Set::iterator uni_char_iter = characters.begin();
       uni_char_iter != characters.end(); ++uni_char_iter) {
    char16 uni_char = *uni_char_iter;
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
    // Add uni_char/word_id entry to the char_words database table.
    if (cache_enabled())
      cache_db_->AddWordToCharWords(uni_char, word_id);
  }
}

void URLIndexPrivateData::RemoveRowFromIndex(const URLRow& row) {
  CacheTransaction cache_transaction(cache_enabled() ? cache_db_.get() : NULL);
  RemoveRowWordsFromIndex(row);
  HistoryID history_id = static_cast<HistoryID>(row.id());
  history_info_map_.erase(history_id);
  word_starts_map_.erase(history_id);
  if (cache_enabled())
    cache_db_->RemoveHistoryIDFromURLs(history_id);
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
    string16 word = word_list_[word_id];
    Char16Set characters = Char16SetFromString16(word);
    for (Char16Set::iterator uni_char_iter = characters.begin();
         uni_char_iter != characters.end(); ++uni_char_iter) {
      char16 uni_char = *uni_char_iter;
      char_word_map_[uni_char].erase(word_id);
      if (char_word_map_[uni_char].empty())
        char_word_map_.erase(uni_char);  // No longer in use.
    }

    // Complete the removal of references to the word.
    word_id_history_map_.erase(word_id);
    word_map_.erase(word);
    word_list_[word_id] = string16();
    available_words_.insert(word_id);
    if (cache_enabled())
      cache_db_->RemoveWordFromWords(word_id);
  }
  if (cache_enabled())
    cache_db_->RemoveHistoryIDFromWordHistory(history_id);
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

void URLIndexPrivateData::ResetSearchTermCache() {
  for (SearchTermCacheMap::iterator iter = search_term_cache_.begin();
       iter != search_term_cache_.end(); ++iter)
    iter->second.used_ = false;
}

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
    string16 uni_word = *iter;
    HistoryIDSet term_history_set = HistoryIDsForTerm(uni_word);
    if (term_history_set.empty()) {
      history_id_set.clear();
      break;
    }
    if (iter == words.begin()) {
      history_id_set.swap(term_history_set);
    } else {
      HistoryIDSet new_history_id_set;
      std::set_intersection(history_id_set.begin(), history_id_set.end(),
                            term_history_set.begin(), term_history_set.end(),
                            std::inserter(new_history_id_set,
                                          new_history_id_set.begin()));
      history_id_set.swap(new_history_id_set);
    }
  }
  return history_id_set;
}

HistoryIDSet URLIndexPrivateData::HistoryIDsForTerm(
    const string16& term) {
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
    string16 leftovers(term);
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
    Char16Set unique_chars;
    std::set_difference(leftover_chars.begin(), leftover_chars.end(),
                        prefix_chars.begin(), prefix_chars.end(),
                        std::inserter(unique_chars, unique_chars.begin()));

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
        WordIDSet new_word_id_set;
        std::set_intersection(word_id_set.begin(), word_id_set.end(),
                              leftover_set.begin(), leftover_set.end(),
                              std::inserter(new_word_id_set,
                                            new_word_id_set.begin()));
        word_id_set.swap(new_word_id_set);
      }
    }

    // We must filter the word list because the resulting word set surely
    // contains words which do not have the search term as a proper subset.
    for (WordIDSet::iterator word_set_iter = word_id_set.begin();
         word_set_iter != word_id_set.end(); ) {
      if (word_list_[*word_set_iter].find(term) == string16::npos)
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
      WordIDSet new_word_id_set;
      std::set_intersection(word_id_set.begin(), word_id_set.end(),
                            char_word_id_set.begin(), char_word_id_set.end(),
                            std::inserter(new_word_id_set,
                                          new_word_id_set.begin()));
      word_id_set.swap(new_word_id_set);
    }
  }
  return word_id_set;
}

bool URLIndexPrivateData::RestoreFromCache(InMemoryURLCacheDatabase* cache_db) {
  DCHECK(cache_db);
  if (!cache_db->RestorePrivateData(this)) {
    cache_db->Reset();
    return false;
  }
  return !Empty() && ValidateConsistency();
}

void URLIndexPrivateData::DeleteOldVersionCacheFile() const {
  if (history_dir_.empty())
    return;
  FilePath path = history_dir_.Append(chrome::kHQPCacheFilename);
  file_util::Delete(path, false);
}

bool URLIndexPrivateData::GetCacheDBPath(FilePath* file_path) {
  DCHECK(file_path);
  if (history_dir_.empty())
    return false;
  *file_path = history_dir_.Append(chrome::kHQPCacheDBFilename);
  return true;
}

void URLIndexPrivateData::SetCacheDatabaseForTesting(
    InMemoryURLCacheDatabase* test_db) {
  cache_db_ = test_db;
}

// static
bool URLIndexPrivateData::URLSchemeIsWhitelisted(
    const GURL& gurl,
    const std::set<std::string>& whitelist) {
  return whitelist.find(gurl.scheme()) != whitelist.end();
}

}  // namespace history
