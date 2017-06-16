// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/test/test_store.h"

#include "base/memory/ptr_util.h"
#include "components/download/internal/entry.h"

namespace download {
namespace test {

TestStore::TestStore() : ready_(false), init_called_(false) {}

TestStore::~TestStore() {}

bool TestStore::IsInitialized() {
  return ready_;
}

void TestStore::Initialize(InitCallback callback) {
  init_called_ = true;
  init_callback_ = std::move(callback);

  if (automatic_callback_response_.has_value())
    TriggerInit(automatic_callback_response_.value(),
                base::MakeUnique<std::vector<Entry>>());
}

void TestStore::Update(const Entry& entry, StoreCallback callback) {
  updated_entries_.push_back(entry);
  update_callback_ = std::move(callback);

  if (automatic_callback_response_.has_value())
    TriggerUpdate(automatic_callback_response_.value());
}

void TestStore::Remove(const std::string& guid, StoreCallback callback) {
  removed_entries_.push_back(guid);
  remove_callback_ = std::move(callback);

  if (automatic_callback_response_.has_value())
    TriggerRemove(automatic_callback_response_.value());
}

void TestStore::AutomaticallyTriggerAllFutureCallbacks(bool success) {
  automatic_callback_response_ = success;
}

void TestStore::TriggerInit(bool success,
                            std::unique_ptr<std::vector<Entry>> entries) {
  ready_ = success;
  DCHECK(init_callback_);
  std::move(init_callback_).Run(success, std::move(entries));
}

void TestStore::TriggerUpdate(bool success) {
  DCHECK(update_callback_);
  std::move(update_callback_).Run(success);
}

void TestStore::TriggerRemove(bool success) {
  DCHECK(remove_callback_);
  std::move(remove_callback_).Run(success);
}

const Entry* TestStore::LastUpdatedEntry() const {
  return &updated_entries_.back();
}

std::string TestStore::LastRemovedEntry() const {
  return removed_entries_.back();
}

}  // namespace test
}  // namespace download
