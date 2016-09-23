// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_black_list_item.h"

#include <algorithm>

#include "components/previews/core/previews_opt_out_store.h"

namespace previews {

PreviewsBlackListItem::OptOutRecord::OptOutRecord(base::Time entry_time,
                                                  bool opt_out)
    : entry_time(entry_time), opt_out(opt_out) {}

PreviewsBlackListItem::PreviewsBlackListItem(
    size_t stored_history_length,
    int opt_out_black_list_threshold,
    base::TimeDelta black_list_duration)
    : max_stored_history_length_(stored_history_length),
      opt_out_black_list_threshold_(opt_out_black_list_threshold),
      max_black_list_duration_(black_list_duration),
      total_opt_out_(0) {}

PreviewsBlackListItem::~PreviewsBlackListItem() {}

void PreviewsBlackListItem::AddPreviewNavigation(bool opt_out,
                                                 base::Time entry_time) {
  DCHECK_LE(opt_out_records_.size(), max_stored_history_length_);
  if (opt_out && (!most_recent_opt_out_time_ ||
                  entry_time > most_recent_opt_out_time_.value())) {
    most_recent_opt_out_time_ = entry_time;
  }
  total_opt_out_ += opt_out ? 1 : 0;

  // Find insert postion to keep the list sorted. Typically, this will be at the
  // end of the list, but for systems with clocks that are not non-decreasing
  // with time, this may not be the end. Linearly search from end to begin.
  auto iter = opt_out_records_.rbegin();
  while (iter != opt_out_records_.rend() && iter->entry_time > entry_time)
    iter++;
  opt_out_records_.emplace(iter.base(), entry_time, opt_out);

  // Remove the oldest entry if the size exceeds the history size.
  if (opt_out_records_.size() > max_stored_history_length_) {
    total_opt_out_ -= opt_out_records_.front().opt_out ? 1 : 0;
    opt_out_records_.pop_front();
  }
  DCHECK_LE(opt_out_records_.size(), max_stored_history_length_);
}

bool PreviewsBlackListItem::IsBlackListed(base::Time now) const {
  DCHECK_LE(opt_out_records_.size(), max_stored_history_length_);
  return most_recent_opt_out_time_ &&
         now - most_recent_opt_out_time_.value() < max_black_list_duration_ &&
         total_opt_out_ >= opt_out_black_list_threshold_;
}

}  // namespace previews
