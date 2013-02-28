// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_query.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/strings/string_split.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_item.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "third_party/icu/public/i18n/unicode/regex.h"

using content::DownloadDangerType;
using content::DownloadItem;

namespace {

// Templatized base::Value::GetAs*().
template <typename T> bool GetAs(const base::Value& in, T* out);
template<> bool GetAs(const base::Value& in, bool* out) {
  return in.GetAsBoolean(out);
}
template<> bool GetAs(const base::Value& in, int* out) {
  return in.GetAsInteger(out);
}
template<> bool GetAs(const base::Value& in, std::string* out) {
  return in.GetAsString(out);
}
template<> bool GetAs(const base::Value& in, string16* out) {
  return in.GetAsString(out);
}

// The next several functions are helpers for making Callbacks that access
// DownloadItem fields.

static bool MatchesQuery(const string16& query, const DownloadItem& item) {
  if (query.empty())
    return true;

  DCHECK_EQ(query, base::i18n::ToLower(query));

  string16 url_raw(UTF8ToUTF16(item.GetOriginalUrl().spec()));
  if (base::i18n::StringSearchIgnoringCaseAndAccents(
          query, url_raw, NULL, NULL)) {
    return true;
  }

  string16 url_formatted = url_raw;
  if (item.GetBrowserContext()) {
    url_formatted = net::FormatUrl(
        item.GetOriginalUrl(),
        content::GetContentClient()->browser()->GetAcceptLangs(
            item.GetBrowserContext()));
  }
  if (base::i18n::StringSearchIgnoringCaseAndAccents(
        query, url_formatted, NULL, NULL)) {
    return true;
  }

  string16 path(item.GetTargetFilePath().LossyDisplayName());
  return base::i18n::StringSearchIgnoringCaseAndAccents(
      query, path, NULL, NULL);
}

static int64 GetStartTimeMsEpoch(const DownloadItem& item) {
  return (item.GetStartTime() - base::Time::UnixEpoch()).InMilliseconds();
}

static int64 GetEndTimeMsEpoch(const DownloadItem& item) {
  return (item.GetEndTime() - base::Time::UnixEpoch()).InMilliseconds();
}

std::string TimeToISO8601(const base::Time& t) {
  base::Time::Exploded exploded;
  t.UTCExplode(&exploded);
  return base::StringPrintf(
      "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", exploded.year, exploded.month,
      exploded.day_of_month, exploded.hour, exploded.minute, exploded.second,
      exploded.millisecond);
}

static std::string GetStartTime(const DownloadItem& item) {
  return TimeToISO8601(item.GetStartTime());
}

static std::string GetEndTime(const DownloadItem& item) {
  return TimeToISO8601(item.GetEndTime());
}

static bool GetDangerAccepted(const DownloadItem& item) {
  return (item.GetDangerType() ==
          content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED);
}

static bool GetExists(const DownloadItem& item) {
  return !item.GetFileExternallyRemoved();
}

static string16 GetFilename(const DownloadItem& item) {
  // This filename will be compared with strings that could be passed in by the
  // user, who only sees LossyDisplayNames.
  return item.GetTargetFilePath().LossyDisplayName();
}

static std::string GetFilenameUTF8(const DownloadItem& item) {
  return UTF16ToUTF8(GetFilename(item));
}

static std::string GetUrl(const DownloadItem& item) {
  return item.GetOriginalUrl().spec();
}

static DownloadItem::DownloadState GetState(const DownloadItem& item) {
  return item.GetState();
}

static DownloadDangerType GetDangerType(const DownloadItem& item) {
  return item.GetDangerType();
}

static int GetReceivedBytes(const DownloadItem& item) {
  return item.GetReceivedBytes();
}

static int GetTotalBytes(const DownloadItem& item) {
  return item.GetTotalBytes();
}

static std::string GetMimeType(const DownloadItem& item) {
  return item.GetMimeType();
}

static bool IsPaused(const DownloadItem& item) {
  return item.IsPaused();
}

enum ComparisonType {LT, EQ, GT};

// Returns true if |item| matches the filter specified by |value|, |cmptype|,
// and |accessor|. |accessor| is conceptually a function that takes a
// DownloadItem and returns one of its fields, which is then compared to
// |value|.
template<typename ValueType>
static bool FieldMatches(
    const ValueType& value,
    ComparisonType cmptype,
    const base::Callback<ValueType(const DownloadItem&)>& accessor,
    const DownloadItem& item) {
  switch (cmptype) {
    case LT: return accessor.Run(item) < value;
    case EQ: return accessor.Run(item) == value;
    case GT: return accessor.Run(item) > value;
  }
  NOTREACHED();
  return false;
}

// Helper for building a Callback to FieldMatches<>().
template <typename ValueType> DownloadQuery::FilterCallback BuildFilter(
    const base::Value& value, ComparisonType cmptype,
    ValueType (*accessor)(const DownloadItem&)) {
  ValueType cpp_value;
  if (!GetAs(value, &cpp_value)) return DownloadQuery::FilterCallback();
  return base::Bind(&FieldMatches<ValueType>, cpp_value, cmptype,
                    base::Bind(accessor));
}

// Returns true if |accessor.Run(item)| matches |pattern|.
static bool FindRegex(
    icu::RegexPattern* pattern,
    const base::Callback<std::string(const DownloadItem&)>& accessor,
    const DownloadItem& item) {
  icu::UnicodeString input(accessor.Run(item).c_str());
  UErrorCode status = U_ZERO_ERROR;
  scoped_ptr<icu::RegexMatcher> matcher(pattern->matcher(input, status));
  return matcher->find() == TRUE;  // Ugh, VS complains bool != UBool.
}

// Helper for building a Callback to FindRegex().
DownloadQuery::FilterCallback BuildRegexFilter(
    const base::Value& regex_value,
    std::string (*accessor)(const DownloadItem&)) {
  std::string regex_str;
  if (!GetAs(regex_value, &regex_str)) return DownloadQuery::FilterCallback();
  UParseError re_err;
  UErrorCode re_status = U_ZERO_ERROR;
  scoped_ptr<icu::RegexPattern> pattern(icu::RegexPattern::compile(
      icu::UnicodeString::fromUTF8(regex_str.c_str()), re_err, re_status));
  if (!U_SUCCESS(re_status)) return DownloadQuery::FilterCallback();
  return base::Bind(&FindRegex, base::Owned(pattern.release()),
                    base::Bind(accessor));
}

// Returns a ComparisonType to indicate whether a field in |left| is less than,
// greater than or equal to the same field in |right|.
template<typename ValueType>
static ComparisonType Compare(
    const base::Callback<ValueType(const DownloadItem&)>& accessor,
    const DownloadItem& left, const DownloadItem& right) {
  ValueType left_value = accessor.Run(left);
  ValueType right_value = accessor.Run(right);
  if (left_value > right_value) return GT;
  if (left_value < right_value) return LT;
  DCHECK_EQ(left_value, right_value);
  return EQ;
}

}  // anonymous namespace

DownloadQuery::DownloadQuery()
  : limit_(kuint32max) {
}

DownloadQuery::~DownloadQuery() {
}

// AddFilter() pushes a new FilterCallback to filters_. Most FilterCallbacks are
// Callbacks to FieldMatches<>(). Search() iterates over given DownloadItems,
// discarding items for which any filter returns false. A DownloadQuery may have
// zero or more FilterCallbacks.

bool DownloadQuery::AddFilter(const DownloadQuery::FilterCallback& value) {
  if (value.is_null()) return false;
  filters_.push_back(value);
  return true;
}

void DownloadQuery::AddFilter(DownloadItem::DownloadState state) {
  AddFilter(base::Bind(&FieldMatches<DownloadItem::DownloadState>, state, EQ,
      base::Bind(&GetState)));
}

void DownloadQuery::AddFilter(DownloadDangerType danger) {
  AddFilter(base::Bind(&FieldMatches<DownloadDangerType>, danger, EQ,
      base::Bind(&GetDangerType)));
}

bool DownloadQuery::AddFilter(DownloadQuery::FilterType type,
                              const base::Value& value) {
  switch (type) {
    case FILTER_BYTES_RECEIVED:
      return AddFilter(BuildFilter<int>(value, EQ, &GetReceivedBytes));
    case FILTER_DANGER_ACCEPTED:
      return AddFilter(BuildFilter<bool>(value, EQ, &GetDangerAccepted));
    case FILTER_EXISTS:
      return AddFilter(BuildFilter<bool>(value, EQ, &GetExists));
    case FILTER_FILENAME:
      return AddFilter(BuildFilter<string16>(value, EQ, &GetFilename));
    case FILTER_FILENAME_REGEX:
      return AddFilter(BuildRegexFilter(value, &GetFilenameUTF8));
    case FILTER_MIME:
      return AddFilter(BuildFilter<std::string>(value, EQ, &GetMimeType));
    case FILTER_PAUSED:
      return AddFilter(BuildFilter<bool>(value, EQ, &IsPaused));
    case FILTER_QUERY: {
      string16 query;
      return GetAs(value, &query) &&
             AddFilter(base::Bind(&MatchesQuery, query));
    }
    case FILTER_ENDED_AFTER:
      return AddFilter(BuildFilter<std::string>(value, GT, &GetEndTime));
    case FILTER_ENDED_BEFORE:
      return AddFilter(BuildFilter<std::string>(value, LT, &GetEndTime));
    case FILTER_END_TIME:
      return AddFilter(BuildFilter<std::string>(value, EQ, &GetEndTime));
    case FILTER_STARTED_AFTER:
      return AddFilter(BuildFilter<std::string>(value, GT, &GetStartTime));
    case FILTER_STARTED_BEFORE:
      return AddFilter(BuildFilter<std::string>(value, LT, &GetStartTime));
    case FILTER_START_TIME:
      return AddFilter(BuildFilter<std::string>(value, EQ, &GetStartTime));
    case FILTER_TOTAL_BYTES:
      return AddFilter(BuildFilter<int>(value, EQ, &GetTotalBytes));
    case FILTER_TOTAL_BYTES_GREATER:
      return AddFilter(BuildFilter<int>(value, GT, &GetTotalBytes));
    case FILTER_TOTAL_BYTES_LESS:
      return AddFilter(BuildFilter<int>(value, LT, &GetTotalBytes));
    case FILTER_URL:
      return AddFilter(BuildFilter<std::string>(value, EQ, &GetUrl));
    case FILTER_URL_REGEX:
      return AddFilter(BuildRegexFilter(value, &GetUrl));
  }
  return false;
}

bool DownloadQuery::Matches(const DownloadItem& item) const {
  for (FilterCallbackVector::const_iterator filter = filters_.begin();
        filter != filters_.end(); ++filter) {
    if (!filter->Run(item))
      return false;
  }
  return true;
}

// AddSorter() creates a Sorter and pushes it onto sorters_. A Sorter is a
// direction and a Callback to Compare<>(). After filtering, Search() makes a
// DownloadComparator functor from the sorters_ and passes the
// DownloadComparator to std::partial_sort. std::partial_sort calls the
// DownloadComparator with different pairs of DownloadItems.  DownloadComparator
// iterates over the sorters until a callback returns ComparisonType LT or GT.
// DownloadComparator returns true or false depending on that ComparisonType and
// the sorter's direction in order to indicate to std::partial_sort whether the
// left item is after or before the right item. If all sorters return EQ, then
// DownloadComparator compares GetId. A DownloadQuery may have zero or more
// Sorters, but there is one DownloadComparator per call to Search().

struct DownloadQuery::Sorter {
  typedef base::Callback<ComparisonType(
      const DownloadItem&, const DownloadItem&)> SortType;

  template<typename ValueType>
  static Sorter Build(DownloadQuery::SortDirection adirection,
                         ValueType (*accessor)(const DownloadItem&)) {
    return Sorter(adirection, base::Bind(&Compare<ValueType>,
        base::Bind(accessor)));
  }

  Sorter(DownloadQuery::SortDirection adirection,
            const SortType& asorter)
    : direction(adirection),
      sorter(asorter) {
  }
  ~Sorter() {}

  DownloadQuery::SortDirection direction;
  SortType sorter;
};

class DownloadQuery::DownloadComparator {
 public:
  explicit DownloadComparator(const DownloadQuery::SorterVector& terms)
    : terms_(terms) {
  }

  // Returns true if |left| sorts before |right|.
  bool operator() (const DownloadItem* left, const DownloadItem* right);

 private:
  const DownloadQuery::SorterVector& terms_;

  // std::sort requires this class to be copyable.
};

bool DownloadQuery::DownloadComparator::operator() (
    const DownloadItem* left, const DownloadItem* right) {
  for (DownloadQuery::SorterVector::const_iterator term = terms_.begin();
       term != terms_.end(); ++term) {
    switch (term->sorter.Run(*left, *right)) {
      case LT: return term->direction == DownloadQuery::ASCENDING;
      case GT: return term->direction == DownloadQuery::DESCENDING;
      case EQ: break;  // break the switch but not the loop
    }
  }
  CHECK_NE(left->GetId(), right->GetId());
  return left->GetId() < right->GetId();
}

void DownloadQuery::AddSorter(DownloadQuery::SortType type,
                              DownloadQuery::SortDirection direction) {
  switch (type) {
    case SORT_END_TIME:
      sorters_.push_back(Sorter::Build<int64>(direction, &GetEndTimeMsEpoch));
      break;
    case SORT_START_TIME:
      sorters_.push_back(Sorter::Build<int64>(direction, &GetStartTimeMsEpoch));
      break;
    case SORT_URL:
      sorters_.push_back(Sorter::Build<std::string>(direction, &GetUrl));
      break;
    case SORT_FILENAME:
      sorters_.push_back(Sorter::Build<string16>(direction, &GetFilename));
      break;
    case SORT_DANGER:
      sorters_.push_back(Sorter::Build<DownloadDangerType>(
          direction, &GetDangerType));
      break;
    case SORT_DANGER_ACCEPTED:
      sorters_.push_back(Sorter::Build<bool>(direction, &GetDangerAccepted));
      break;
    case SORT_EXISTS:
      sorters_.push_back(Sorter::Build<bool>(direction, &GetExists));
      break;
    case SORT_STATE:
      sorters_.push_back(Sorter::Build<DownloadItem::DownloadState>(
          direction, &GetState));
      break;
    case SORT_PAUSED:
      sorters_.push_back(Sorter::Build<bool>(direction, &IsPaused));
      break;
    case SORT_MIME:
      sorters_.push_back(Sorter::Build<std::string>(direction, &GetMimeType));
      break;
    case SORT_BYTES_RECEIVED:
      sorters_.push_back(Sorter::Build<int>(direction, &GetReceivedBytes));
      break;
    case SORT_TOTAL_BYTES:
      sorters_.push_back(Sorter::Build<int>(direction, &GetTotalBytes));
      break;
  }
}

void DownloadQuery::FinishSearch(DownloadQuery::DownloadVector* results) const {
  if (!sorters_.empty())
    std::partial_sort(results->begin(),
                      results->begin() + std::min(limit_, results->size()),
                      results->end(),
                      DownloadComparator(sorters_));
  if (results->size() > limit_)
    results->resize(limit_);
}
