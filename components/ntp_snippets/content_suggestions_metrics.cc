// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_metrics.h"

#include <string>
#include <type_traits>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/template_util.h"

namespace ntp_snippets {
namespace metrics {

namespace {

const int kMaxSuggestionsPerCategory = 10;
const int kMaxSuggestionsTotal = 50;

const char kHistogramCountOnNtpOpened[] =
    "NewTabPage.ContentSuggestions.CountOnNtpOpened";
const char kHistogramShown[] = "NewTabPage.ContentSuggestions.Shown";
const char kHistogramShownAge[] = "NewTabPage.ContentSuggestions.ShownAge";
const char kHistogramShownScore[] = "NewTabPage.ContentSuggestions.ShownScore";
const char kHistogramOpened[] = "NewTabPage.ContentSuggestions.Opened";
const char kHistogramOpenedAge[] = "NewTabPage.ContentSuggestions.OpenedAge";
const char kHistogramOpenedScore[] =
    "NewTabPage.ContentSuggestions.OpenedScore";
const char kHistogramOpenDisposition[] =
    "NewTabPage.ContentSuggestions.OpenDisposition";
const char kHistogramMenuOpened[] = "NewTabPage.ContentSuggestions.MenuOpened";
const char kHistogramMenuOpenedAge[] =
    "NewTabPage.ContentSuggestions.MenuOpenedAge";
const char kHistogramMenuOpenedScore[] =
    "NewTabPage.ContentSuggestions.MenuOpenedScore";
const char kHistogramDismissedUnvisited[] =
    "NewTabPage.ContentSuggestions.DismissedUnvisited";
const char kHistogramDismissedVisited[] =
    "NewTabPage.ContentSuggestions.DismissedVisited";
const char kHistogramVisitDuration[] =
    "NewTabPage.ContentSuggestions.VisitDuration";
const char kHistogramMoreButtonShown[] =
    "NewTabPage.ContentSuggestions.MoreButtonShown";
const char kHistogramMoreButtonClicked[] =
    "NewTabPage.ContentSuggestions.MoreButtonClicked";

const char kPerCategoryHistogramFormat[] = "%s.%s";

// Each suffix here should correspond to an entry under histogram suffix
// ContentSuggestionCategory in histograms.xml.
std::string GetCategorySuffix(Category category) {
  static_assert(
      std::is_same<decltype(category.id()), typename base::underlying_type<
                                                KnownCategories>::type>::value,
      "KnownCategories must have the same underlying type as category.id()");
  // Note: Since the underlying type of KnownCategories is int, it's legal to
  // cast from int to KnownCategories, even if the given value isn't listed in
  // the enumeration. The switch still makes sure that all known values are
  // listed here.
  KnownCategories known_category = static_cast<KnownCategories>(category.id());
  switch (known_category) {
    case KnownCategories::RECENT_TABS:
      return "RecentTabs";
    case KnownCategories::DOWNLOADS:
      return "Downloads";
    case KnownCategories::BOOKMARKS:
      return "Bookmarks";
    case KnownCategories::PHYSICAL_WEB_PAGES:
      return "PhysicalWeb";
    case KnownCategories::FOREIGN_TABS:
      return "ForeignTabs";
    case KnownCategories::ARTICLES:
      return "Articles";
    case KnownCategories::LOCAL_CATEGORIES_COUNT:
    case KnownCategories::REMOTE_CATEGORIES_OFFSET:
      NOTREACHED();
      break;
  }
  // All other (unknown) categories go into a single "Experimental" bucket.
  return "Experimental";
}

std::string GetCategoryHistogramName(const char* base_name, Category category) {
  return base::StringPrintf(kPerCategoryHistogramFormat, base_name,
                            GetCategorySuffix(category).c_str());
}

// This corresponds to UMA_HISTOGRAM_ENUMERATION, for use with dynamic histogram
// names.
void UmaHistogramEnumeration(const std::string& name,
                             int value,
                             int boundary_value) {
  base::LinearHistogram::FactoryGet(
      name, 1, boundary_value, boundary_value + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(value);
}

// This corresponds to UMA_HISTOGRAM_LONG_TIMES for use with dynamic histogram
// names.
void UmaHistogramLongTimes(const std::string& name,
                           const base::TimeDelta& value) {
  base::Histogram::FactoryTimeGet(
      name, base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromHours(1),
      50, base::HistogramBase::kUmaTargetedHistogramFlag)
      ->AddTime(value);
}

// This corresponds to UMA_HISTOGRAM_CUSTOM_TIMES (with min/max appropriate
// for the age of suggestions) for use with dynamic histogram names.
void UmaHistogramAge(const std::string& name, const base::TimeDelta& value) {
  base::Histogram::FactoryTimeGet(
      name, base::TimeDelta::FromSeconds(1), base::TimeDelta::FromDays(7), 100,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->AddTime(value);
}

// This corresponds to UMA_HISTOGRAM_CUSTOM_COUNTS (with min/max appropriate
// for the score of suggestions) for use with dynamic histogram names.
void UmaHistogramScore(const std::string& name, float value) {
  base::Histogram::FactoryGet(name, 1, 100000, 50,
                              base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(value);
}

void LogCategoryHistogramEnumeration(const char* base_name,
                                     Category category,
                                     int value,
                                     int boundary_value) {
  std::string name = GetCategoryHistogramName(base_name, category);
  // Since the histogram name is dynamic, we can't use the regular macro.
  UmaHistogramEnumeration(name, value, boundary_value);
}

void LogCategoryHistogramLongTimes(const char* base_name,
                                   Category category,
                                   const base::TimeDelta& value) {
  std::string name = GetCategoryHistogramName(base_name, category);
  // Since the histogram name is dynamic, we can't use the regular macro.
  UmaHistogramLongTimes(name, value);
}

void LogCategoryHistogramAge(const char* base_name,
                             Category category,
                             const base::TimeDelta& value) {
  std::string name = GetCategoryHistogramName(base_name, category);
  // Since the histogram name is dynamic, we can't use the regular macro.
  UmaHistogramAge(name, value);
}

void LogCategoryHistogramScore(const char* base_name,
                               Category category,
                               float score) {
  std::string name = GetCategoryHistogramName(base_name, category);
  // Since the histogram name is dynamic, we can't use the regular macro.
  UmaHistogramScore(name, score);
}

}  // namespace

void OnPageShown(
    const std::vector<std::pair<Category, int>>& suggestions_per_category) {
  int suggestions_total = 0;
  for (const std::pair<Category, int>& item : suggestions_per_category) {
    LogCategoryHistogramEnumeration(kHistogramCountOnNtpOpened, item.first,
                                    item.second, kMaxSuggestionsPerCategory);
    suggestions_total += item.second;
  }

  UMA_HISTOGRAM_ENUMERATION(kHistogramCountOnNtpOpened, suggestions_total,
                            kMaxSuggestionsTotal);
}

void OnSuggestionShown(int global_position,
                       Category category,
                       int category_position,
                       base::Time publish_date,
                       float score) {
  UMA_HISTOGRAM_ENUMERATION(kHistogramShown, global_position,
                            kMaxSuggestionsTotal);
  LogCategoryHistogramEnumeration(kHistogramShown, category, category_position,
                                  kMaxSuggestionsPerCategory);

  base::TimeDelta age = base::Time::Now() - publish_date;
  LogCategoryHistogramAge(kHistogramShownAge, category, age);

  LogCategoryHistogramScore(kHistogramShownScore, category, score);
}

void OnSuggestionOpened(int global_position,
                        Category category,
                        int category_position,
                        base::Time publish_date,
                        float score,
                        WindowOpenDisposition disposition) {
  UMA_HISTOGRAM_ENUMERATION(kHistogramOpened, global_position,
                            kMaxSuggestionsTotal);
  LogCategoryHistogramEnumeration(kHistogramOpened, category, category_position,
                                  kMaxSuggestionsPerCategory);

  base::TimeDelta age = base::Time::Now() - publish_date;
  LogCategoryHistogramAge(kHistogramOpenedAge, category, age);

  LogCategoryHistogramScore(kHistogramOpenedScore, category, score);

  UMA_HISTOGRAM_ENUMERATION(
      kHistogramOpenDisposition, static_cast<int>(disposition),
      static_cast<int>(WindowOpenDisposition::MAX_VALUE) + 1);
  LogCategoryHistogramEnumeration(
      kHistogramOpenDisposition, category, static_cast<int>(disposition),
      static_cast<int>(WindowOpenDisposition::MAX_VALUE) + 1);
}

void OnSuggestionMenuOpened(int global_position,
                            Category category,
                            int category_position,
                            base::Time publish_date,
                            float score) {
  UMA_HISTOGRAM_ENUMERATION(kHistogramMenuOpened, global_position,
                            kMaxSuggestionsTotal);
  LogCategoryHistogramEnumeration(kHistogramMenuOpened, category,
                                  category_position,
                                  kMaxSuggestionsPerCategory);

  base::TimeDelta age = base::Time::Now() - publish_date;
  LogCategoryHistogramAge(kHistogramMenuOpenedAge, category, age);

  LogCategoryHistogramScore(kHistogramMenuOpenedScore, category, score);
}

void OnSuggestionDismissed(int global_position,
                           Category category,
                           int category_position,
                           bool visited) {
  if (visited) {
    UMA_HISTOGRAM_ENUMERATION(kHistogramDismissedVisited, global_position,
                              kMaxSuggestionsTotal);
    LogCategoryHistogramEnumeration(kHistogramDismissedVisited, category,
                                    category_position,
                                    kMaxSuggestionsPerCategory);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kHistogramDismissedUnvisited, global_position,
                              kMaxSuggestionsTotal);
    LogCategoryHistogramEnumeration(kHistogramDismissedUnvisited, category,
                                    category_position,
                                    kMaxSuggestionsPerCategory);
  }
}

void OnSuggestionTargetVisited(Category category, base::TimeDelta visit_time) {
  LogCategoryHistogramLongTimes(kHistogramVisitDuration, category, visit_time);
}

void OnMoreButtonShown(Category category, int position) {
  // The "more" card can appear in addition to the actual suggestions, so add
  // one extra bucket to this histogram.
  LogCategoryHistogramEnumeration(kHistogramMoreButtonShown, category, position,
                                  kMaxSuggestionsPerCategory + 1);
}

void OnMoreButtonClicked(Category category, int position) {
  // The "more" card can appear in addition to the actual suggestions, so add
  // one extra bucket to this histogram.
  LogCategoryHistogramEnumeration(kHistogramMoreButtonClicked, category,
                                  position, kMaxSuggestionsPerCategory + 1);
}

}  // namespace metrics
}  // namespace ntp_snippets
