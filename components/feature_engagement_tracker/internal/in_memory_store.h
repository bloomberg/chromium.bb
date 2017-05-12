// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_IN_MEMORY_STORE_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_IN_MEMORY_STORE_H_

#include <vector>

#include "base/macros.h"
#include "components/feature_engagement_tracker/internal/store.h"

namespace feature_engagement_tracker {
// An InMemoryStore provides a DB layer that stores all data in-memory.
// All data is made available to this class during construction, and can be
// loaded once by a caller. All calls to WriteEvent(...) are ignored.
class InMemoryStore : public Store {
 public:
  explicit InMemoryStore(std::unique_ptr<std::vector<Event>> events);
  InMemoryStore();
  ~InMemoryStore() override;

  // Store implementation.
  void Load(const OnLoadedCallback& callback) override;
  bool IsReady() const override;
  void WriteEvent(const Event& event) override;
  void DeleteEvent(const std::string& event_name) override;

 protected:
  // Posts the result of loading and sets up the ready state.
  // Protected and virtual for testing.
  virtual void HandleLoadResult(const OnLoadedCallback& callback, bool success);

 private:
  // All events that this in-memory store was constructed with. This will be
  // reset when Load(...) is called.
  std::unique_ptr<std::vector<Event>> events_;

  // Whether the store is ready or not. It is true after Load(...) has been
  // invoked.
  bool ready_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryStore);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_IN_MEMORY_STORE_H_
