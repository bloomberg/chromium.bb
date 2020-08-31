// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/invalidation_adapter.h"

namespace syncer {

InvalidationAdapter::InvalidationAdapter(const Invalidation& invalidation)
    : invalidation_(invalidation) {}

InvalidationAdapter::~InvalidationAdapter() {}

bool InvalidationAdapter::IsUnknownVersion() const {
  return invalidation_.is_unknown_version();
}

const std::string& InvalidationAdapter::GetPayload() const {
  return invalidation_.payload();
}

int64_t InvalidationAdapter::GetVersion() const {
  return invalidation_.version();
}

void InvalidationAdapter::Acknowledge() {
  invalidation_.Acknowledge();
}

void InvalidationAdapter::Drop() {
  invalidation_.Drop();
}

}  // namespace syncer
