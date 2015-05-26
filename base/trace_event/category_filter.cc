// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/category_filter.h"

#include "base/strings/string_tokenizer.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"

namespace base {
namespace trace_event {

namespace {

const char kSyntheticDelayCategoryFilterPrefix[] = "DELAY(";

}  // namespace

bool CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
    const std::string& str) {
  return  str.empty() ||
          str.at(0) == ' ' ||
          str.at(str.length() - 1) == ' ';
}

CategoryFilter::CategoryFilter(const std::string& filter_string) {
  if (!filter_string.empty())
    Initialize(filter_string);
  else
    Initialize(CategoryFilter::kDefaultCategoryFilterString);
}

CategoryFilter::CategoryFilter() {
    Initialize(CategoryFilter::kDefaultCategoryFilterString);
}

CategoryFilter::CategoryFilter(const CategoryFilter& cf)
    : included_(cf.included_),
      disabled_(cf.disabled_),
      excluded_(cf.excluded_),
      delays_(cf.delays_) {
}

CategoryFilter::~CategoryFilter() {
}

CategoryFilter& CategoryFilter::operator=(const CategoryFilter& rhs) {
  if (this == &rhs)
    return *this;

  included_ = rhs.included_;
  disabled_ = rhs.disabled_;
  excluded_ = rhs.excluded_;
  delays_ = rhs.delays_;
  return *this;
}

void CategoryFilter::Initialize(const std::string& filter_string) {
  // Tokenize list of categories, delimited by ','.
  StringTokenizer tokens(filter_string, ",");
  // Add each token to the appropriate list (included_,excluded_).
  while (tokens.GetNext()) {
    std::string category = tokens.token();
    // Ignore empty categories.
    if (category.empty())
      continue;
    // Synthetic delays are of the form 'DELAY(delay;option;option;...)'.
    if (category.find(kSyntheticDelayCategoryFilterPrefix) == 0 &&
        category.at(category.size() - 1) == ')') {
      category = category.substr(
          strlen(kSyntheticDelayCategoryFilterPrefix),
          category.size() - strlen(kSyntheticDelayCategoryFilterPrefix) - 1);
      size_t name_length = category.find(';');
      if (name_length != std::string::npos && name_length > 0 &&
          name_length != category.size() - 1) {
        delays_.push_back(category);
      }
    } else if (category.at(0) == '-') {
      // Excluded categories start with '-'.
      // Remove '-' from category string.
      category = category.substr(1);
      excluded_.push_back(category);
    } else if (category.compare(0, strlen(TRACE_DISABLED_BY_DEFAULT("")),
                                TRACE_DISABLED_BY_DEFAULT("")) == 0) {
      disabled_.push_back(category);
    } else {
      included_.push_back(category);
    }
  }
}

void CategoryFilter::WriteString(const StringList& values,
                                 std::string* out,
                                 bool included) const {
  bool prepend_comma = !out->empty();
  int token_cnt = 0;
  for (StringList::const_iterator ci = values.begin();
       ci != values.end(); ++ci) {
    if (token_cnt > 0 || prepend_comma)
      StringAppendF(out, ",");
    StringAppendF(out, "%s%s", (included ? "" : "-"), ci->c_str());
    ++token_cnt;
  }
}

void CategoryFilter::WriteString(const StringList& delays,
                                 std::string* out) const {
  bool prepend_comma = !out->empty();
  int token_cnt = 0;
  for (StringList::const_iterator ci = delays.begin();
       ci != delays.end(); ++ci) {
    if (token_cnt > 0 || prepend_comma)
      StringAppendF(out, ",");
    StringAppendF(out, "%s%s)", kSyntheticDelayCategoryFilterPrefix,
                  ci->c_str());
    ++token_cnt;
  }
}

std::string CategoryFilter::ToString() const {
  std::string filter_string;
  WriteString(included_, &filter_string, true);
  WriteString(disabled_, &filter_string, true);
  WriteString(excluded_, &filter_string, false);
  WriteString(delays_, &filter_string);
  return filter_string;
}

bool CategoryFilter::IsCategoryGroupEnabled(
    const char* category_group_name) const {
  // TraceLog should call this method only as  part of enabling/disabling
  // categories.

  bool had_enabled_by_default = false;
  DCHECK(category_group_name);
  CStringTokenizer category_group_tokens(
      category_group_name, category_group_name + strlen(category_group_name),
      ",");
  while (category_group_tokens.GetNext()) {
    std::string category_group_token = category_group_tokens.token();
    // Don't allow empty tokens, nor tokens with leading or trailing space.
    DCHECK(!CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
               category_group_token))
        << "Disallowed category string";
    if (IsCategoryEnabled(category_group_token.c_str())) {
      return true;
    }
    if (!MatchPattern(category_group_token.c_str(),
                      TRACE_DISABLED_BY_DEFAULT("*")))
      had_enabled_by_default = true;
  }
  // Do a second pass to check for explicitly disabled categories
  // (those explicitly enabled have priority due to first pass).
  category_group_tokens.Reset();
  bool category_group_disabled = false;
  while (category_group_tokens.GetNext()) {
    std::string category_group_token = category_group_tokens.token();
    for (StringList::const_iterator ci = excluded_.begin();
         ci != excluded_.end(); ++ci) {
      if (MatchPattern(category_group_token.c_str(), ci->c_str())) {
        // Current token of category_group_name is present in excluded_list.
        // Flag the exclusion and proceed further to check if any of the
        // remaining categories of category_group_name is not present in the
        // excluded_ list.
        category_group_disabled = true;
        break;
      }
      // One of the category of category_group_name is not present in
      // excluded_ list. So, it has to be included_ list. Enable the
      // category_group_name for recording.
      category_group_disabled = false;
    }
    // One of the categories present in category_group_name is not present in
    // excluded_ list. Implies this category_group_name group can be enabled
    // for recording, since one of its groups is enabled for recording.
    if (!category_group_disabled)
      break;
  }
  // If the category group is not excluded, and there are no included patterns
  // we consider this category group enabled, as long as it had categories
  // other than disabled-by-default.
  return !category_group_disabled &&
         included_.empty() && had_enabled_by_default;
}

bool CategoryFilter::IsCategoryEnabled(const char* category_name) const {
  StringList::const_iterator ci;

  // Check the disabled- filters and the disabled-* wildcard first so that a
  // "*" filter does not include the disabled.
  for (ci = disabled_.begin(); ci != disabled_.end(); ++ci) {
    if (MatchPattern(category_name, ci->c_str()))
      return true;
  }

  if (MatchPattern(category_name, TRACE_DISABLED_BY_DEFAULT("*")))
    return false;

  for (ci = included_.begin(); ci != included_.end(); ++ci) {
    if (MatchPattern(category_name, ci->c_str()))
      return true;
  }

  return false;
}

bool CategoryFilter::HasIncludedPatterns() const {
  return !included_.empty();
}

void CategoryFilter::Merge(const CategoryFilter& nested_filter) {
  // Keep included patterns only if both filters have an included entry.
  // Otherwise, one of the filter was specifying "*" and we want to honour the
  // broadest filter.
  if (HasIncludedPatterns() && nested_filter.HasIncludedPatterns()) {
    included_.insert(included_.end(),
                     nested_filter.included_.begin(),
                     nested_filter.included_.end());
  } else {
    included_.clear();
  }

  disabled_.insert(disabled_.end(),
                   nested_filter.disabled_.begin(),
                   nested_filter.disabled_.end());
  excluded_.insert(excluded_.end(),
                   nested_filter.excluded_.begin(),
                   nested_filter.excluded_.end());
  delays_.insert(delays_.end(),
                 nested_filter.delays_.begin(),
                 nested_filter.delays_.end());
}

void CategoryFilter::Clear() {
  included_.clear();
  disabled_.clear();
  excluded_.clear();
}

const CategoryFilter::StringList&
    CategoryFilter::GetSyntheticDelayValues() const {
  return delays_;
}

}  // namespace trace_event
}  // namespace base
