// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/category_filter.h"

#include "base/strings/string_tokenizer.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"

namespace base {
namespace trace_event {

bool CategoryFilter::IsEmptyOrContainsLeadingOrTrailingWhitespace(
    const std::string& str) {
  return  TraceConfig::IsEmptyOrContainsLeadingOrTrailingWhitespace(str);
}

CategoryFilter::CategoryFilter(const std::string& filter_string)
    : config_(new TraceConfig(filter_string, "")) {
}

CategoryFilter::CategoryFilter()
    : config_(new TraceConfig()) {
}

CategoryFilter::CategoryFilter(const CategoryFilter& cf)
    : config_(new TraceConfig(*cf.config_)) {
}

CategoryFilter::~CategoryFilter() {
}

CategoryFilter& CategoryFilter::operator=(const CategoryFilter& rhs) {
  if (this == &rhs)
    return *this;

  config_.reset(new TraceConfig(*rhs.config_));
  return *this;
}

std::string CategoryFilter::ToString() const {
  return config_->ToCategoryFilterString();
}

bool CategoryFilter::IsCategoryGroupEnabled(
    const char* category_group_name) const {
  return config_->IsCategoryGroupEnabled(category_group_name);
}

bool CategoryFilter::IsCategoryEnabled(const char* category_name) const {
  return config_->IsCategoryEnabled(category_name);
}

bool CategoryFilter::HasIncludedPatterns() const {
  return config_->HasIncludedPatterns();
}

void CategoryFilter::Merge(const CategoryFilter& nested_filter) {
  config_->Merge(*nested_filter.config_);
}

void CategoryFilter::Clear() {
  config_->included_categories_.clear();
  config_->disabled_categories_.clear();
  config_->excluded_categories_.clear();
}

const CategoryFilter::StringList&
    CategoryFilter::GetSyntheticDelayValues() const {
  return config_->GetSyntheticDelayValues();
}

}  // namespace trace_event
}  // namespace base
