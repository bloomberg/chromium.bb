// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/in_memory_store.h"

#include "base/feature_list.h"
#include "components/feature_engagement_tracker/internal/store.h"

namespace feature_engagement_tracker {

InMemoryStore::InMemoryStore() : Store(), ready_(false) {}

InMemoryStore::~InMemoryStore() = default;

void InMemoryStore::Load(const OnLoadedCallback& callback) {
  // TODO(nyquist): Post result back to callback.
  ready_ = true;
}

bool InMemoryStore::IsReady() const {
  return ready_;
}

}  // namespace feature_engagement_tracker
