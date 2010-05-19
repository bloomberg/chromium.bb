// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "url_row_type.h"

using base::Time;

namespace history {

void URLRow::Swap(URLRow* other) {
  std::swap(id_, other->id_);
  url_.Swap(&other->url_);
  title_.swap(other->title_);
  std::swap(visit_count_, other->visit_count_);
  std::swap(typed_count_, other->typed_count_);
  std::swap(last_visit_, other->last_visit_);
  std::swap(hidden_, other->hidden_);
  std::swap(favicon_id_, other->favicon_id_);
}

void URLRow::Initialize() {
  id_ = 0;
  visit_count_ = false;
  typed_count_ = false;
  last_visit_ = Time();
  hidden_ = false;
  favicon_id_ = 0;
}

}  // namespace history
