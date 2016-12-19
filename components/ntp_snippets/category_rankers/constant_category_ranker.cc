// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category_rankers/constant_category_ranker.h"

#include "base/stl_util.h"

namespace ntp_snippets {

ConstantCategoryRanker::ConstantCategoryRanker() {
  // Add all local categories in a fixed order.
  AppendKnownCategory(KnownCategories::DOWNLOADS);
  AppendKnownCategory(KnownCategories::RECENT_TABS);
  AppendKnownCategory(KnownCategories::FOREIGN_TABS);
  AppendKnownCategory(KnownCategories::BOOKMARKS);
  AppendKnownCategory(KnownCategories::PHYSICAL_WEB_PAGES);

  DCHECK_EQ(static_cast<size_t>(KnownCategories::LOCAL_CATEGORIES_COUNT),
            ordered_categories_.size());

  // Known remote categories come after. Other remote categories will be ordered
  // after these depending on when providers notify us about them using
  // AppendCategoryIfNecessary.
  // TODO(treib): Consider not adding ARTICLES here, so that providers can
  // define the order themselves.
  AppendKnownCategory(KnownCategories::ARTICLES);
}

ConstantCategoryRanker::~ConstantCategoryRanker() = default;

bool ConstantCategoryRanker::Compare(Category left, Category right) const {
  if (!base::ContainsValue(ordered_categories_, left)) {
    LOG(DFATAL) << "The category with ID " << left.id()
                << " has not been added using AppendCategoryIfNecessary.";
  }
  if (!base::ContainsValue(ordered_categories_, right)) {
    LOG(DFATAL) << "The category with ID " << right.id()
                << " has not been added using AppendCategoryIfNecessary.";
  }
  if (left == right) {
    return false;
  }
  for (Category category : ordered_categories_) {
    if (category == left) {
      return true;
    }
    if (category == right) {
      return false;
    }
  }
  // This fallback is provided only to satisfy "Compare" contract if by mistake
  // categories are not added using AppendCategoryIfNecessary. One should not
  // rely on this, instead the order must be defined explicitly using
  // AppendCategoryIfNecessary.
  return left.id() < right.id();
}

void ConstantCategoryRanker::ClearHistory(base::Time begin, base::Time end) {
  // Ignored, because this implementation doesn't store any history-related
  // data.
}

void ConstantCategoryRanker::AppendCategoryIfNecessary(Category category) {
  if (!base::ContainsValue(ordered_categories_, category)) {
    ordered_categories_.push_back(category);
  }
}

void ConstantCategoryRanker::OnSuggestionOpened(Category category) {
  // Ignored. The order is constant.
}

void ConstantCategoryRanker::AppendKnownCategory(
    KnownCategories known_category) {
  Category category = Category::FromKnownCategory(known_category);
  DCHECK(!base::ContainsValue(ordered_categories_, category));
  ordered_categories_.push_back(category);
}

}  // namespace ntp_snippets
