// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category_rankers/click_based_category_ranker.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/ntp_snippets/category_rankers/constant_category_ranker.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ntp_snippets {

namespace {

// In order to increase stability and predictability of the order, an extra
// level of "confidence" is required before moving a category upwards. In other
// words, the category is moved not when it reaches the previous one, but rather
// when it leads by some amount. We refer to this required extra "confidence" as
// a passing margin. Each position has its own passing margin. The category is
// moved upwards (i.e. passes another category) when it has at least passing
// margin of the previous category position more clicks.
const int kPassingMargin = 5;

// The first categories get more attention and, therefore, here more stability
// is needed. The passing margin of such categories is increased and they are
// referred to as top categories (with extra margin). Only category position
// defines whether a category is top, but not its content.
const int kNumTopCategoriesWithExtraMargin = 3;

// The increase of passing margin for each top category compared to the next
// category (e.g. the first top category has passing margin larger by this value
// than the second top category, the last top category has it larger by this
// value than the first non-top category).
const int kExtraPassingMargin = 2;

// Number of positions by which a dismissed category is downgraded.
const int kDismissedCategoryPenalty = 1;

const char kCategoryIdKey[] = "category";
const char kClicksKey[] = "clicks";

}  // namespace

ClickBasedCategoryRanker::ClickBasedCategoryRanker(PrefService* pref_service)
    : pref_service_(pref_service) {
  if (!ReadOrderFromPrefs(&ordered_categories_)) {
    // TODO(crbug.com/676273): Handle adding new hardcoded KnownCategories to
    // existing order from prefs. Currently such new category is completely
    // ignored and may be never shown.
    RestoreDefaultOrder();
  }
}

ClickBasedCategoryRanker::~ClickBasedCategoryRanker() = default;

bool ClickBasedCategoryRanker::Compare(Category left, Category right) const {
  if (!ContainsCategory(left)) {
    LOG(DFATAL) << "The category with ID " << left.id()
                << " has not been added using AppendCategoryIfNecessary.";
  }
  if (!ContainsCategory(right)) {
    LOG(DFATAL) << "The category with ID " << right.id()
                << " has not been added using AppendCategoryIfNecessary.";
  }
  if (left == right) {
    return false;
  }
  for (const RankedCategory& ranked_category : ordered_categories_) {
    if (ranked_category.category == left) {
      return true;
    }
    if (ranked_category.category == right) {
      return false;
    }
  }
  // This fallback is provided only to satisfy "Compare" contract if by mistake
  // categories are not added using AppendCategoryIfNecessary. One should not
  // rely on this, instead the order must be defined explicitly using
  // AppendCategoryIfNecessary.
  return left.id() < right.id();
}

void ClickBasedCategoryRanker::ClearHistory(base::Time begin, base::Time end) {
  // TODO(crbug.com/675953): Preserve remote categories with 0 counts instead of
  // removing them.
  RestoreDefaultOrder();
}

void ClickBasedCategoryRanker::AppendCategoryIfNecessary(Category category) {
  if (!ContainsCategory(category)) {
    ordered_categories_.push_back(RankedCategory(category, /*clicks=*/0));
  }
}

void ClickBasedCategoryRanker::OnSuggestionOpened(Category category) {
  if (!ContainsCategory(category)) {
    LOG(DFATAL) << "The category with ID " << category.id()
                << " has not been added using AppendCategoryIfNecessary.";
    return;
  }

  std::vector<RankedCategory>::iterator current = FindCategory(category);
  current->clicks++;

  // TODO(crbug.com/676279): Prevent overflow.
  if (current != ordered_categories_.begin()) {
    std::vector<RankedCategory>::iterator previous = current - 1;
    const int passing_margin = GetPositionPassingMargin(previous);
    if (current->clicks >= previous->clicks + passing_margin) {
      // It is intended to move only by one position at a time in order to avoid
      // dramatic changes, which could confuse the user.
      std::swap(*current, *previous);
    }
  }

  StoreOrderToPrefs(ordered_categories_);
}

void ClickBasedCategoryRanker::OnCategoryDismissed(Category category) {
  if (!ContainsCategory(category)) {
    LOG(DFATAL) << "The category with ID " << category.id()
                << " has not been added using AppendCategoryIfNecessary.";
    return;
  }

  std::vector<RankedCategory>::iterator current = FindCategory(category);
  for (int downgrade = 0; downgrade < kDismissedCategoryPenalty; ++downgrade) {
    std::vector<RankedCategory>::iterator next = current + 1;
    if (next == ordered_categories_.end()) {
      break;
    }
    std::swap(*current, *next);
    current = next;
  }

  int next_clicks = 0;
  std::vector<RankedCategory>::iterator next = current + 1;
  if (next != ordered_categories_.end()) {
    next_clicks = next->clicks;
  }

  current->clicks = std::max(next_clicks - kPassingMargin, 0);
  StoreOrderToPrefs(ordered_categories_);
}

// static
void ClickBasedCategoryRanker::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kClickBasedCategoryRankerOrderWithClicks);
}

// static
int ClickBasedCategoryRanker::GetPassingMargin() {
  return kPassingMargin;
}

// static
int ClickBasedCategoryRanker::GetNumTopCategoriesWithExtraMargin() {
  return kNumTopCategoriesWithExtraMargin;
}

// static
int ClickBasedCategoryRanker::GetDismissedCategoryPenalty() {
  return kDismissedCategoryPenalty;
}

ClickBasedCategoryRanker::RankedCategory::RankedCategory(Category category,
                                                         int clicks)
    : category(category), clicks(clicks) {}

// Returns passing margin for a given position taking into account whether it is
// a top category.
int ClickBasedCategoryRanker::GetPositionPassingMargin(
    std::vector<RankedCategory>::const_iterator category_position) const {
  int index = category_position - ordered_categories_.cbegin();
  int passing_margin_increase = 0;
  if (index < kNumTopCategoriesWithExtraMargin) {
    passing_margin_increase =
        kExtraPassingMargin * (kNumTopCategoriesWithExtraMargin - index);
  }
  return kPassingMargin + passing_margin_increase;
}

void ClickBasedCategoryRanker::RestoreDefaultOrder() {
  ordered_categories_.clear();

  std::vector<KnownCategories> ordered_known_categories =
      ConstantCategoryRanker::GetKnownCategoriesDefaultOrder();

  for (KnownCategories known_category : ordered_known_categories) {
    AppendKnownCategory(known_category);
  }

  StoreOrderToPrefs(ordered_categories_);
}

void ClickBasedCategoryRanker::AppendKnownCategory(
    KnownCategories known_category) {
  Category category = Category::FromKnownCategory(known_category);
  DCHECK(!ContainsCategory(category));
  ordered_categories_.push_back(RankedCategory(category, /*clicks=*/0));
}

bool ClickBasedCategoryRanker::ReadOrderFromPrefs(
    std::vector<RankedCategory>* result_categories) {
  result_categories->clear();
  const base::ListValue* list =
      pref_service_->GetList(prefs::kClickBasedCategoryRankerOrderWithClicks);
  if (!list || list->GetSize() == 0) {
    return false;
  }

  for (const std::unique_ptr<base::Value>& value : *list) {
    base::DictionaryValue* dictionary;
    if (!value->GetAsDictionary(&dictionary)) {
      LOG(DFATAL) << "Failed to parse category data from prefs param "
                  << prefs::kClickBasedCategoryRankerOrderWithClicks
                  << " into dictionary.";
      return false;
    }
    int category_id, clicks;
    if (!dictionary->GetInteger(kCategoryIdKey, &category_id)) {
      LOG(DFATAL) << "Dictionary does not have '" << kCategoryIdKey << "' key.";
      return false;
    }
    if (!dictionary->GetInteger(kClicksKey, &clicks)) {
      LOG(DFATAL) << "Dictionary does not have '" << kClicksKey << "' key.";
      return false;
    }
    Category category = Category::FromIDValue(category_id);
    result_categories->push_back(RankedCategory(category, clicks));
  }
  return true;
}

void ClickBasedCategoryRanker::StoreOrderToPrefs(
    const std::vector<RankedCategory>& ordered_categories) {
  base::ListValue list;
  for (const RankedCategory& category : ordered_categories) {
    auto dictionary = base::MakeUnique<base::DictionaryValue>();
    dictionary->SetInteger(kCategoryIdKey, category.category.id());
    dictionary->SetInteger(kClicksKey, category.clicks);
    list.Append(std::move(dictionary));
  }
  pref_service_->Set(prefs::kClickBasedCategoryRankerOrderWithClicks, list);
}

std::vector<ClickBasedCategoryRanker::RankedCategory>::iterator
ClickBasedCategoryRanker::FindCategory(Category category) {
  return std::find_if(ordered_categories_.begin(), ordered_categories_.end(),
                      [category](const RankedCategory& ranked_category) {
                        return category == ranked_category.category;
                      });
}

bool ClickBasedCategoryRanker::ContainsCategory(Category category) const {
  for (const auto& ranked_category : ordered_categories_) {
    if (category == ranked_category.category) {
      return true;
    }
  }
  return false;
}

}  // namespace ntp_snippets
