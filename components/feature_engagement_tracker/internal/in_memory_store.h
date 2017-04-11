// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_IN_MEMORY_STORE_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_IN_MEMORY_STORE_H_

#include "base/macros.h"
#include "components/feature_engagement_tracker/internal/store.h"

namespace feature_engagement_tracker {

// An InMemoryStore provides a DB layer that stores all data in-memory.
class InMemoryStore : public Store {
 public:
  InMemoryStore();
  ~InMemoryStore() override;

  // Store implementation.
  void Load(const OnLoadedCallback& callback) override;
  bool IsReady() const override;

 private:
  // Whether the store is ready or not. It is true after Load(...) has been
  // invoked.
  bool ready_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryStore);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_IN_MEMORY_STORE_H_
