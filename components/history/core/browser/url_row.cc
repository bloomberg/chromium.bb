// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/url_row.h"

#include <algorithm>

namespace history {

URLRow::URLRow() {
}

URLRow::URLRow(const GURL& url) : url_(url) {
}

URLRow::URLRow(const GURL& url, URLID id) : id_(id), url_(url) {}

URLRow::URLRow(const URLRow& other) = default;

// TODO(bug 706963) this should be implemented as "= default" when Android
// toolchain is updated.
URLRow::URLRow(URLRow&& other) noexcept
    : id_(other.id_),
      url_(std::move(other.url_)),
      title_(std::move(other.title_)),
      visit_count_(other.visit_count_),
      typed_count_(other.typed_count_),
      last_visit_(other.last_visit_),
      hidden_(other.hidden_) {}

URLRow::~URLRow() {
}

URLRow& URLRow::operator=(const URLRow& other) = default;

void URLRow::Swap(URLRow* other) {
  std::swap(id_, other->id_);
  url_.Swap(&other->url_);
  title_.swap(other->title_);
  std::swap(visit_count_, other->visit_count_);
  std::swap(typed_count_, other->typed_count_);
  std::swap(last_visit_, other->last_visit_);
  std::swap(hidden_, other->hidden_);
}

URLResult::URLResult() {}

URLResult::URLResult(const GURL& url, base::Time visit_time)
    : URLRow(url), visit_time_(visit_time) {}

URLResult::URLResult(const URLRow& url_row) : URLRow(url_row) {}

URLResult::URLResult(const URLResult& other) = default;

// TODO(bug 706963) this should be implemented as "= default" when Android
// toolchain is updated.
URLResult::URLResult(URLResult&& other) noexcept
    : URLRow(std::move(other)),
      visit_time_(other.visit_time_),
      snippet_(std::move(other.snippet_)),
      title_match_positions_(std::move(other.title_match_positions_)),
      blocked_visit_(other.blocked_visit_) {}

URLResult::~URLResult() {
}

URLResult& URLResult::operator=(const URLResult&) = default;

void URLResult::SwapResult(URLResult* other) {
  URLRow::Swap(other);
  std::swap(visit_time_, other->visit_time_);
  snippet_.Swap(&other->snippet_);
  title_match_positions_.swap(other->title_match_positions_);
  std::swap(blocked_visit_, other->blocked_visit_);
}

// static
bool URLResult::CompareVisitTime(const URLResult& lhs, const URLResult& rhs) {
  return lhs.visit_time() > rhs.visit_time();
}

}  // namespace history
