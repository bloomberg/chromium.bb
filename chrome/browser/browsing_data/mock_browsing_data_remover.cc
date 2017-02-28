// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_remover.h"

#include "content/public/browser/browsing_data_filter_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
base::Time some_time;
}

MockBrowsingDataRemover::MockBrowsingDataRemover(
    content::BrowserContext* context) {}

MockBrowsingDataRemover::~MockBrowsingDataRemover() {
  DCHECK(!expected_calls_.size()) << "Expectations were set but not verified.";
}

void MockBrowsingDataRemover::Shutdown() {}

void MockBrowsingDataRemover::Remove(const base::Time& delete_begin,
                                     const base::Time& delete_end,
                                     int remove_mask,
                                     int origin_type_mask) {
  RemoveInternal(delete_begin, delete_end, remove_mask, origin_type_mask,
                 std::unique_ptr<content::BrowsingDataFilterBuilder>(),
                 nullptr);
}

void MockBrowsingDataRemover::RemoveAndReply(const base::Time& delete_begin,
                                             const base::Time& delete_end,
                                             int remove_mask,
                                             int origin_type_mask,
                                             Observer* observer) {
  DCHECK(observer);
  RemoveInternal(delete_begin, delete_end, remove_mask, origin_type_mask,
                 std::unique_ptr<content::BrowsingDataFilterBuilder>(),
                 observer);
}

void MockBrowsingDataRemover::RemoveWithFilter(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    int origin_type_mask,
    std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder) {
  DCHECK_EQ(0, remove_mask & ~FILTERABLE_DATATYPES);
  DCHECK(filter_builder);
  RemoveInternal(delete_begin, delete_end, remove_mask, origin_type_mask,
                 std::move(filter_builder), nullptr);
}

void MockBrowsingDataRemover::RemoveWithFilterAndReply(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    int origin_type_mask,
    std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder,
    Observer* observer) {
  DCHECK_EQ(0, remove_mask & ~FILTERABLE_DATATYPES);
  DCHECK(filter_builder);
  DCHECK(observer);
  RemoveInternal(delete_begin, delete_end, remove_mask, origin_type_mask,
                 std::move(filter_builder), observer);
}

void MockBrowsingDataRemover::RemoveInternal(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    int origin_type_mask,
    std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder,
    BrowsingDataRemover::Observer* observer) {
  actual_calls_.emplace_back(delete_begin, delete_end, remove_mask,
                             origin_type_mask, std::move(filter_builder),
                             true /* should_compare_filter */);

  // |observer| is not recorded in |actual_calls_| to be compared with
  // expectations, because it's created internally in ClearSiteData() and
  // it's unknown to this. However, it is tested implicitly, because we use
  // it for the completion callback, so an incorrect |observer| will fail
  // the test by waiting for the callback forever.
  DCHECK(observer);
  observer->OnBrowsingDataRemoverDone();
}

void MockBrowsingDataRemover::AddObserver(Observer* observer) {}

void MockBrowsingDataRemover::RemoveObserver(Observer* observer) {}

const base::Time& MockBrowsingDataRemover::GetLastUsedBeginTime() {
  NOTIMPLEMENTED();
  return some_time;
}

const base::Time& MockBrowsingDataRemover::GetLastUsedEndTime() {
  NOTIMPLEMENTED();
  return some_time;
}

int MockBrowsingDataRemover::GetLastUsedRemovalMask() {
  NOTIMPLEMENTED();
  return -1;
}

int MockBrowsingDataRemover::GetLastUsedOriginTypeMask() {
  NOTIMPLEMENTED();
  return -1;
}

void MockBrowsingDataRemover::SetEmbedderDelegate(
    std::unique_ptr<BrowsingDataRemoverDelegate> embedder_delegate) {
  NOTIMPLEMENTED();
}

BrowsingDataRemoverDelegate* MockBrowsingDataRemover::GetEmbedderDelegate()
    const {
  NOTIMPLEMENTED();
  return nullptr;
}

void MockBrowsingDataRemover::ExpectCall(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    int origin_type_mask,
    std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder) {
  expected_calls_.emplace_back(delete_begin, delete_end, remove_mask,
                               origin_type_mask, std::move(filter_builder),
                               true /* should_compare_filter */);
}

void MockBrowsingDataRemover::ExpectCallDontCareAboutFilterBuilder(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    int origin_type_mask) {
  expected_calls_.emplace_back(
      delete_begin, delete_end, remove_mask, origin_type_mask,
      std::unique_ptr<content::BrowsingDataFilterBuilder>(),
      false /* should_compare_filter */);
}

void MockBrowsingDataRemover::VerifyAndClearExpectations() {
  EXPECT_EQ(expected_calls_, actual_calls_);
  expected_calls_.clear();
  actual_calls_.clear();
}

MockBrowsingDataRemover::CallParameters::CallParameters(
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
MockBrowsingDataRemover::CallParameters::~CallParameters() {}

bool MockBrowsingDataRemover::CallParameters::operator==(
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
