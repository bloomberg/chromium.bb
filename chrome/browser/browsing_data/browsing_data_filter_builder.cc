// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/browsing_data/browsing_data_filter_builder.h"

namespace {

bool NoopFilter(const GURL& url) {
  return true;
}

}  // namespace

BrowsingDataFilterBuilder::BrowsingDataFilterBuilder(Mode mode) : mode_(mode) {}

BrowsingDataFilterBuilder::~BrowsingDataFilterBuilder() {}

void BrowsingDataFilterBuilder::SetMode(Mode mode) {
  mode_ = mode;
}

bool BrowsingDataFilterBuilder::IsEmptyBlacklist() const {
  return mode_ == Mode::BLACKLIST && IsEmpty();
}

// static
base::Callback<bool(const GURL&)> BrowsingDataFilterBuilder::BuildNoopFilter() {
  return base::Bind(&NoopFilter);
}
