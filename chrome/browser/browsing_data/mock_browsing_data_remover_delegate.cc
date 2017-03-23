// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_remover_delegate.h"

#include "base/callback.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

MockBrowsingDataRemoverDelegate::MockBrowsingDataRemoverDelegate() {}

MockBrowsingDataRemoverDelegate::~MockBrowsingDataRemoverDelegate() {
  DCHECK(!expected_calls_.size()) << "Expectations were set but not verified.";
}

bool MockBrowsingDataRemoverDelegate::DoesOriginMatchEmbedderMask(
    int origin_type_mask,
    const GURL& origin,
    storage::SpecialStoragePolicy* special_storage_policy) const {
  return false;
}

void MockBrowsingDataRemoverDelegate::RemoveEmbedderData(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    const content::BrowsingDataFilterBuilder& filter_builder,
    int origin_type_mask,
    const base::Closure& callback) {
  actual_calls_.emplace_back(delete_begin, delete_end, remove_mask,
                             origin_type_mask, filter_builder.Copy(),
                             true /* should_compare_filter */);
  callback.Run();
}

void MockBrowsingDataRemoverDelegate::ExpectCall(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    int origin_type_mask,
    const content::BrowsingDataFilterBuilder& filter_builder) {
  expected_calls_.emplace_back(delete_begin, delete_end, remove_mask,
                               origin_type_mask, filter_builder.Copy(),
                               true /* should_compare_filter */);
}

void MockBrowsingDataRemoverDelegate::ExpectCallDontCareAboutFilterBuilder(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    int origin_type_mask) {
  expected_calls_.emplace_back(
      delete_begin, delete_end, remove_mask, origin_type_mask,
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::BLACKLIST),
      false /* should_compare_filter */);
}

void MockBrowsingDataRemoverDelegate::VerifyAndClearExpectations() {
  EXPECT_EQ(expected_calls_, actual_calls_);
  expected_calls_.clear();
  actual_calls_.clear();
}

MockBrowsingDataRemoverDelegate::CallParameters::CallParameters(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    int origin_type_mask,
    std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder,
    bool should_compare_filter)
    : delete_begin_(delete_begin),
      delete_end_(delete_end),
      remove_mask_(remove_mask),
      origin_type_mask_(origin_type_mask),
      filter_builder_(std::move(filter_builder)),
      should_compare_filter_(should_compare_filter) {}

MockBrowsingDataRemoverDelegate::CallParameters::~CallParameters() {}

bool MockBrowsingDataRemoverDelegate::CallParameters::operator==(
    const CallParameters& other) const {
  const CallParameters& a = *this;
  const CallParameters& b = other;

  if (a.delete_begin_ != b.delete_begin_ || a.delete_end_ != b.delete_end_ ||
      a.remove_mask_ != b.remove_mask_ ||
      a.origin_type_mask_ != b.origin_type_mask_) {
    return false;
  }

  if (!a.should_compare_filter_ || !b.should_compare_filter_)
    return true;
  return *a.filter_builder_ == *b.filter_builder_;
}
