// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/leveldb_wrapper.mojom.h"
#include "content/common/storage_partition_service.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Mock LevelDBWrapper that records all read and write events. It also
// implements a mock StoragePartitionService.
class MockLevelDBWrapper : public mojom::StoragePartitionService,
                           public mojom::LevelDBWrapper {
 public:
  using ResultCallback = base::OnceCallback<void(bool)>;

  MockLevelDBWrapper();
  ~MockLevelDBWrapper() override;

  // StoragePartitionService implementation:
  void OpenLocalStorage(const url::Origin& origin,
                        mojom::LevelDBWrapperRequest database) override;

  // LevelDBWrapper implementation:
  void AddObserver(mojom::LevelDBObserverAssociatedPtrInfo observer) override;

  void Put(const std::vector<uint8_t>& key,
           const std::vector<uint8_t>& value,
           const base::Optional<std::vector<uint8_t>>& client_old_value,
           const std::string& source,
           PutCallback callback) override;

  void Delete(const std::vector<uint8_t>& key,
              const base::Optional<std::vector<uint8_t>>& client_old_value,
              const std::string& source,
              DeleteCallback callback) override;

  void DeleteAll(const std::string& source,
                 DeleteAllCallback callback) override;

  void Get(const std::vector<uint8_t>& key, GetCallback callback) override;

  void GetAll(
      mojom::LevelDBWrapperGetAllCallbackAssociatedPtrInfo complete_callback,
      GetAllCallback callback) override;

  // Methods and members for use by test fixtures.
  bool HasBindings() { return !bindings_.empty(); }

  void ResetObservations() {
    observed_get_all_ = false;
    observed_put_ = false;
    observed_delete_ = false;
    observed_delete_all_ = false;
    observed_key_.clear();
    observed_value_.clear();
    observed_source_.clear();
  }

  void CompleteAllPendingCallbacks() {
    while (!pending_callbacks_.empty())
      CompleteOnePendingCallback(true);
  }

  void CompleteOnePendingCallback(bool success) {
    ASSERT_TRUE(!pending_callbacks_.empty());
    std::move(pending_callbacks_.front()).Run(success);
    pending_callbacks_.pop_front();
  }

  void Flush() { bindings_.FlushForTesting(); }

  void CloseAllBindings() { bindings_.CloseAllBindings(); }

  const std::list<ResultCallback>& pending_callbacks() const {
    return pending_callbacks_;
  }

  bool observed_get_all() const { return observed_get_all_; }
  bool observed_put() const { return observed_put_; }
  bool observed_delete() const { return observed_delete_; }
  bool observed_delete_all() const { return observed_delete_all_; }
  const std::vector<uint8_t>& observed_key() const { return observed_key_; }
  const std::vector<uint8_t>& observed_value() const { return observed_value_; }
  const std::string& observed_source() const { return observed_source_; }

  std::map<std::vector<uint8_t>, std::vector<uint8_t>>&
  mutable_get_all_return_values() {
    return get_all_return_values_;
  }

 private:
  std::list<ResultCallback> pending_callbacks_;
  bool observed_get_all_ = false;
  bool observed_put_ = false;
  bool observed_delete_ = false;
  bool observed_delete_all_ = false;
  std::vector<uint8_t> observed_key_;
  std::vector<uint8_t> observed_value_;
  std::string observed_source_;

  std::map<std::vector<uint8_t>, std::vector<uint8_t>> get_all_return_values_;

  mojo::BindingSet<mojom::LevelDBWrapper> bindings_;
};

}  // namespace content
