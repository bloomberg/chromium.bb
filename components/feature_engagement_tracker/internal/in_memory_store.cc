// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/in_memory_store.h"

#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/feature_engagement_tracker/internal/store.h"

namespace feature_engagement_tracker {

InMemoryStore::InMemoryStore(std::unique_ptr<std::vector<Event>> events)
    : Store(), events_(std::move(events)), ready_(false) {}

InMemoryStore::InMemoryStore()
    : InMemoryStore(base::MakeUnique<std::vector<Event>>()) {}

InMemoryStore::~InMemoryStore() = default;

void InMemoryStore::Load(const OnLoadedCallback& callback) {
  HandleLoadResult(callback, true);
}

bool InMemoryStore::IsReady() const {
  return ready_;
}

void InMemoryStore::WriteEvent(const Event& event) {
  // Intentionally ignore all writes.
}

void InMemoryStore::DeleteEvent(const std::string& event_name) {
  // Intentionally ignore all deletes.
}

void InMemoryStore::HandleLoadResult(const OnLoadedCallback& callback,
                                     bool success) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, success, base::Passed(&events_)));
  ready_ = success;
}

}  // namespace feature_engagement_tracker
