// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_CATEGORY_FILTER_H_
#define BASE_TRACE_EVENT_CATEGORY_FILTER_H_

#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/trace_event/trace_config.h"

namespace base {
namespace trace_event {

class TraceConfig;

class BASE_EXPORT CategoryFilter {
 public:
  typedef std::vector<std::string> StringList;

  // The default category filter, used when none is provided.
  // Allows all categories through, except if they end in the suffix 'Debug' or
  // 'Test'.
  static const char kDefaultCategoryFilterString[];

  // |filter_string| is a comma-delimited list of category wildcards.
  // A category can have an optional '-' prefix to make it an excluded category.
  // All the same rules apply above, so for example, having both included and
  // excluded categories in the same list would not be supported.
  //
  // Example: CategoryFilter"test_MyTest*");
  // Example: CategoryFilter("test_MyTest*,test_OtherStuff");
  // Example: CategoryFilter("-excluded_category1,-excluded_category2");
  // Example: CategoryFilter("-*,webkit"); would disable everything but webkit.
  // Example: CategoryFilter("-webkit"); would enable everything but webkit.
  //
  // Category filters can also be used to configure synthetic delays.
  //
  // Example: CategoryFilter("DELAY(gpu.PresentingFrame;16)"); would make swap
  //          buffers always take at least 16 ms.
  // Example: CategoryFilter("DELAY(gpu.PresentingFrame;16;oneshot)"); would
  //          make swap buffers take at least 16 ms the first time it is
  //          called.
  // Example: CategoryFilter("DELAY(gpu.PresentingFrame;16;alternating)");
  //          would make swap buffers take at least 16 ms every other time it
  //          is called.
  explicit CategoryFilter(const std::string& filter_string);

  CategoryFilter();

  CategoryFilter(const CategoryFilter& cf);

  ~CategoryFilter();

  CategoryFilter& operator=(const CategoryFilter& rhs);

  // Writes the string representation of the CategoryFilter. This is a comma
  // separated string, similar in nature to the one used to determine
  // enabled/disabled category patterns, except here there is an arbitrary
  // order, included categories go first, then excluded categories. Excluded
  // categories are distinguished from included categories by the prefix '-'.
  std::string ToString() const;

  // Returns true if at least one category in the list is enabled by this
  // category filter.
  bool IsCategoryGroupEnabled(const char* category_group) const;

  // Return a list of the synthetic delays specified in this category filter.
  const StringList& GetSyntheticDelayValues() const;

  // Merges nested_filter with the current CategoryFilter
  void Merge(const CategoryFilter& nested_filter);

  // Clears both included/excluded pattern lists. This would be equivalent to
  // creating a CategoryFilter with an empty string, through the constructor.
  // i.e: CategoryFilter().
  //
  // When using an empty filter, all categories are considered included as we
  // are not excluding anything.
  void Clear();

 private:
  FRIEND_TEST_ALL_PREFIXES(CategoryFilterTest, CategoryFilter);
  friend class TraceConfig;

  // Returns true if category is enable according to this filter.
  bool IsCategoryEnabled(const char* category_name) const;

  static bool IsEmptyOrContainsLeadingOrTrailingWhitespace(
      const std::string& str);

  void Initialize(const std::string& filter_string);
  bool HasIncludedPatterns() const;

  scoped_ptr<TraceConfig> config_;
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_CATEGORY_FILTER_H_
