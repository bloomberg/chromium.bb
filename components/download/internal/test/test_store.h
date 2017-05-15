// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_TEST_TEST_STORE_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_TEST_TEST_STORE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/download/internal/store.h"

namespace download {

struct Entry;

namespace test {

class TestStore : public Store {
 public:
  TestStore();
  ~TestStore() override;

  // Store implementation.
  bool IsInitialized() override;
  void Initialize(InitCallback callback) override;
  void Destroy(StoreCallback callback) override;
  void Update(const Entry& entry, StoreCallback callback) override;
  void Remove(const std::string& guid, StoreCallback callback) override;

  // Callback trigger methods.
  void TriggerInit(bool success, std::unique_ptr<std::vector<Entry>> entries);
  void TriggerDestroy(bool success);
  void TriggerUpdate(bool success);
  void TriggerRemove(bool success);

  // Parameter access methods.
  const Entry* LastUpdatedEntry() const;
  std::string LastRemovedEntry() const;
  bool init_called() const { return init_called_; }
  bool destroy_called() const { return destroy_called_; }
  const std::vector<Entry>& updated_entries() const { return updated_entries_; }
  const std::vector<std::string>& removed_entries() const {
    return removed_entries_;
  }

 private:
  bool ready_;

  bool init_called_;
  bool destroy_called_;

  std::vector<Entry> updated_entries_;
  std::vector<std::string> removed_entries_;

  InitCallback init_callback_;
  StoreCallback destroy_callback_;
  StoreCallback update_callback_;
  StoreCallback remove_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestStore);
};

}  // namespace test
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_TEST_TEST_STORE_H_
