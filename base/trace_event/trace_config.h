// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TRACE_CONFIG_H_
#define BASE_TRACE_EVENT_TRACE_CONFIG_H_

#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/gtest_prod_util.h"
#include "base/trace_event/category_filter.h"
#include "base/trace_event/trace_options.h"
#include "base/values.h"

namespace base {
namespace trace_event {

class CategoryFilter;

class BASE_EXPORT TraceConfig {
 public:
  typedef std::vector<std::string> StringList;

  TraceConfig();

  // Create TraceConfig object from CategoryFilter and TraceOptions.
  TraceConfig(const CategoryFilter& cf, const TraceOptions& options);

  // Create TraceConfig object from category filter and trace options strings.
  TraceConfig(const std::string& category_filter_string,
              const std::string& trace_options_string);

  // |config_string| is a dictionary formatted as a JSON string, containing both
  // category filters and trace options.
  explicit TraceConfig(const std::string& config_string);

  ~TraceConfig();

  // Return a list of the synthetic delays specified in this category filter.
  const StringList& GetSyntheticDelayValues() const;

  // Writes the string representation of the TraceConfig. The string is JSON
  // formatted.
  std::string ToString() const;

  // Write the string representation of the CategoryFilter part.
  std::string ToCategoryFilterString() const;

  // Returns true if at least one category in the list is enabled by this
  // trace config.
  bool IsCategoryGroupEnabled(const char* category_group) const;

  // Merges config with the current TraceConfig
  void Merge(const TraceConfig& config);

  void Clear();

 private:
  FRIEND_TEST_ALL_PREFIXES(TraceConfigTest, TraceConfigFromValidLegacyStrings);
  FRIEND_TEST_ALL_PREFIXES(TraceConfigTest,
                           TraceConfigFromInvalidLegacyStrings);
  FRIEND_TEST_ALL_PREFIXES(TraceConfigTest, ConstructDefaultTraceConfig);
  FRIEND_TEST_ALL_PREFIXES(TraceConfigTest, TraceConfigFromValidString);
  FRIEND_TEST_ALL_PREFIXES(TraceConfigTest, TraceConfigFromInvalidString);
  FRIEND_TEST_ALL_PREFIXES(TraceConfigTest, MergingTraceConfigs);
  FRIEND_TEST_ALL_PREFIXES(TraceConfigTest, IsCategoryGroupEnabled);
  FRIEND_TEST_ALL_PREFIXES(TraceConfigTest,
                           IsEmptyOrContainsLeadingOrTrailingWhitespace);
  friend class CategoryFilter;

  // TODO(zhenw): remove COPY and ASSIGN after CategoryFilter is removed.
  explicit TraceConfig(const TraceConfig& tc);
  TraceConfig& operator=(const TraceConfig& rhs);

  // The default trace config, used when none is provided.
  // Allows all non-disabled-by-default categories through, except if they end
  // in the suffix 'Debug' or 'Test'.
  void InitializeDefault();

  void Initialize(const std::string& config_string);

  void SetCategoriesFromList(StringList& categories,
                             const base::ListValue& list);
  void SetSyntheticDelaysFromList(StringList& delays,
                                  const base::ListValue& list);
  void AddCategoryToDict(base::DictionaryValue& dict,
                         const char* param,
                         const StringList& categories) const;

  // Convert TraceConfig to the dict representation of the TraceConfig.
  void ToDict(base::DictionaryValue& dict) const;

  std::string ToTraceOptionsString() const;

  void WriteCategoryFilterString(const StringList& values,
                                 std::string* out,
                                 bool included) const;
  void WriteCategoryFilterString(const StringList& delays,
                                 std::string* out) const;

  // Returns true if category is enable according to this trace config.
  bool IsCategoryEnabled(const char* category_name) const;

  static bool IsEmptyOrContainsLeadingOrTrailingWhitespace(
      const std::string& str);

  bool HasIncludedPatterns() const;

  TraceRecordMode record_mode_;
  bool enable_sampling_ : 1;
  bool enable_systrace_ : 1;
  bool enable_argument_filter_ : 1;

  StringList included_categories_;
  StringList disabled_categories_;
  StringList excluded_categories_;
  StringList synthetic_delays_;
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_TRACE_CONFIG_H_
