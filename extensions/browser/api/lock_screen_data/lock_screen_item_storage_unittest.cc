// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/lock_screen_data/lock_screen_item_storage.h"

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task_scheduler/post_task.h"
#include "base/time/time.h"
#include "chromeos/login/login_state.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/api/lock_screen_data/data_item.h"
#include "extensions/browser/api/lock_screen_data/operation_result.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/api/lock_screen_data.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace lock_screen_data {

namespace {

const char kTestUserIdHash[] = "user_id_hash";
const char kTestSymmetricKey[] = "fake_symmetric_key";

void RecordCreateResult(OperationResult* result_out,
                        const DataItem** item_out,
                        OperationResult result,
                        const DataItem* item) {
  *result_out = result;
  *item_out = item;
}

void RecordGetAllItemsResult(std::vector<std::string>* items_out,
                             const std::vector<const DataItem*>& items) {
  items_out->clear();
  for (const DataItem* item : items)
    items_out->push_back(item->id());
}

void RecordWriteResult(OperationResult* result_out, OperationResult result) {
  *result_out = result;
}

void RecordReadResult(OperationResult* result_out,
                      std::unique_ptr<std::vector<char>>* content_out,
                      OperationResult result,
                      std::unique_ptr<std::vector<char>> content) {
  *result_out = result;
  *content_out = std::move(content);
}

class TestEventRouter : public extensions::EventRouter {
 public:
  explicit TestEventRouter(content::BrowserContext* context)
      : extensions::EventRouter(context, nullptr) {}
  ~TestEventRouter() override = default;

  bool ExtensionHasEventListener(const std::string& extension_id,
                                 const std::string& event_name) const override {
    return event_name ==
           extensions::api::lock_screen_data::OnDataItemsAvailable::kEventName;
  }

  void BroadcastEvent(std::unique_ptr<extensions::Event> event) override {}

  void DispatchEventToExtension(
      const std::string& extension_id,
      std::unique_ptr<extensions::Event> event) override {
    if (event->event_name !=
        extensions::api::lock_screen_data::OnDataItemsAvailable::kEventName) {
      return;
    }
    ASSERT_TRUE(event->event_args);
    const base::Value* arg_value = nullptr;
    ASSERT_TRUE(event->event_args->Get(0, &arg_value));
    ASSERT_TRUE(arg_value);

    std::unique_ptr<extensions::api::lock_screen_data::DataItemsAvailableEvent>
        event_args = extensions::api::lock_screen_data::
            DataItemsAvailableEvent::FromValue(*arg_value);
    ASSERT_TRUE(event_args);
    was_locked_values_.push_back(event_args->was_locked);
  }

  const std::vector<bool>& was_locked_values() const {
    return was_locked_values_;
  }

  void ClearWasLockedValues() { was_locked_values_.clear(); }

 private:
  std::vector<bool> was_locked_values_;

  DISALLOW_COPY_AND_ASSIGN(TestEventRouter);
};

std::unique_ptr<KeyedService> TestEventRouterFactoryFunction(
    content::BrowserContext* context) {
  return std::make_unique<TestEventRouter>(context);
}

// Keeps track of all fake data items registered during a test.
class ItemRegistry {
 public:
  explicit ItemRegistry(const std::string& extension_id)
      : extension_id_(extension_id) {}
  ~ItemRegistry() = default;

  // Adds a new item to set of registered items.
  bool Add(const std::string& item_id) {
    EXPECT_FALSE(items_.count(item_id));

    if (!allow_new_)
      return false;
    items_.insert(item_id);
    return true;
  }

  // Removes an item from the set of registered items.
  void Remove(const std::string& item_id) {
    ASSERT_TRUE(items_.count(item_id));
    items_.erase(item_id);
  }

  void RemoveAll() { items_.clear(); }

  // Gets the set of registered data items.
  void HandleGetRequest(const DataItem::RegisteredValuesCallback& callback) {
    if (!throttle_get_) {
      RunCallback(callback);
      return;
    }

    ASSERT_TRUE(pending_callback_.is_null());
    pending_callback_ = callback;
  }

  // Completes a pending |HandleGetRequest| request.
  void RunPendingCallback() {
    ASSERT_FALSE(pending_callback_.is_null());
    DataItem::RegisteredValuesCallback callback = pending_callback_;
    pending_callback_.Reset();
    RunCallback(callback);
  }

  bool HasPendingCallback() const { return !pending_callback_.is_null(); }

  void set_allow_new(bool allow_new) { allow_new_ = allow_new; }
  void set_fail(bool fail) { fail_ = fail; }
  void set_throttle_get(bool throttle_get) { throttle_get_ = throttle_get; }

 private:
  void RunCallback(const DataItem::RegisteredValuesCallback& callback) {
    callback.Run(fail_ ? OperationResult::kFailed : OperationResult::kSuccess,
                 ItemsToValue());
  }

  std::unique_ptr<base::DictionaryValue> ItemsToValue() {
    if (fail_)
      return nullptr;

    std::unique_ptr<base::DictionaryValue> result =
        std::make_unique<base::DictionaryValue>();

    for (const std::string& item_id : items_)
      result->Set(item_id, std::make_unique<base::DictionaryValue>());

    return result;
  }

  const std::string extension_id_;
  // Whether data item registration should succeed.
  bool allow_new_ = true;
  // Whether data item retrievals should fail.
  bool fail_ = false;
  // Whether the data item retrivals should be throttled. If set,
  // |HandleGetRequest| callback will be saved to |pending_callback_| without
  // returning. Test will have to invoke |RunPendingCallback| in order to
  // complete the request.
  bool throttle_get_ = false;

  DataItem::RegisteredValuesCallback pending_callback_;
  // Set of registered item ids.
  std::set<std::string> items_;

  DISALLOW_COPY_AND_ASSIGN(ItemRegistry);
};

// Keeps track of all operations requested from the test data item.
// The operations will remain in pending state until completed by calling
// CompleteNextOperation.
// This is owned by the test class, but data items created during the test have
// a reference to the object. More than one data item can have a reference to
// this - data items with the same ID will get the same operation queue.
class OperationQueue {
 public:
  enum class OperationType { kWrite, kRead, kDelete };

  struct PendingOperation {
    explicit PendingOperation(OperationType type) : type(type) {}

    OperationType type;
    // Set only for write - data to be written.
    std::vector<char> data;

    // Callback for write operation.
    DataItem::WriteCallback write_callback;

    // Callback for read operation.
    DataItem::ReadCallback read_callback;

    // Callback for delete operation.
    DataItem::WriteCallback delete_callback;
  };

  OperationQueue(const std::string& id, ItemRegistry* item_registry)
      : id_(id), item_registry_(item_registry) {}

  ~OperationQueue() = default;

  void Register(const DataItem::WriteCallback& callback) {
    bool registered = item_registry_->Add(id_);
    callback.Run(registered ? OperationResult::kSuccess
                            : OperationResult::kFailed);
  }

  void AddWrite(const std::vector<char>& data,
                const DataItem::WriteCallback& callback) {
    PendingOperation operation(OperationType::kWrite);
    operation.data = data;
    operation.write_callback = callback;

    pending_operations_.emplace(std::move(operation));
  }

  void AddRead(const DataItem::ReadCallback& callback) {
    PendingOperation operation(OperationType::kRead);
    operation.read_callback = callback;

    pending_operations_.emplace(std::move(operation));
  }

  void AddDelete(const DataItem::WriteCallback& callback) {
    PendingOperation operation(OperationType::kDelete);
    operation.delete_callback = callback;

    pending_operations_.emplace(std::move(operation));
  }

  // Completes the next pendig operation.
  // |expected_type| - the expected type of the next operation - this will fail
  //     if the operation does not match.
  // |result| - the intended operation result.
  void CompleteNextOperation(OperationType expected_type,
                             OperationResult result) {
    ASSERT_FALSE(pending_operations_.empty());
    ASSERT_FALSE(deleted_);

    const PendingOperation& operation = pending_operations_.front();

    ASSERT_EQ(expected_type, operation.type);

    switch (expected_type) {
      case OperationType::kWrite: {
        if (result == OperationResult::kSuccess)
          content_ = operation.data;
        DataItem::WriteCallback callback = operation.write_callback;
        pending_operations_.pop();
        callback.Run(result);
        break;
      }
      case OperationType::kDelete: {
        if (result == OperationResult::kSuccess) {
          deleted_ = true;
          item_registry_->Remove(id_);
          content_ = std::vector<char>();
        }

        DataItem::WriteCallback callback = operation.delete_callback;
        pending_operations_.pop();
        callback.Run(result);
        break;
      }
      case OperationType::kRead: {
        std::unique_ptr<std::vector<char>> result_data;
        if (result == OperationResult::kSuccess) {
          result_data = std::make_unique<std::vector<char>>(content_.begin(),
                                                            content_.end());
        }

        DataItem::ReadCallback callback = operation.read_callback;
        pending_operations_.pop();
        callback.Run(result, std::move(result_data));
        break;
      }
      default:
        ADD_FAILURE() << "Unexpected operation";
        return;
    }
  }

  bool HasPendingOperations() const { return !pending_operations_.empty(); }

  bool deleted() const { return deleted_; }

  const std::vector<char>& content() const { return content_; }

 private:
  std::string id_;
  ItemRegistry* item_registry_;
  base::queue<PendingOperation> pending_operations_;
  std::vector<char> content_;
  bool deleted_ = false;

  DISALLOW_COPY_AND_ASSIGN(OperationQueue);
};

// Test data item - routes all requests to the OperationQueue provided through
// the ctor - the owning test is responsible for completing the started
// operations.
class TestDataItem : public DataItem {
 public:
  // |operations| - Operation queue used by this data item - not owned by this,
  // and expected to outlive this object.
  TestDataItem(const std::string& id,
               const std::string& extension_id,
               const std::string& crypto_key,
               OperationQueue* operations)
      : DataItem(id, extension_id, nullptr, nullptr, nullptr, crypto_key),
        operations_(operations) {}

  ~TestDataItem() override = default;

  void Register(const WriteCallback& callback) override {
    operations_->Register(callback);
  }

  void Write(const std::vector<char>& data,
             const WriteCallback& callback) override {
    operations_->AddWrite(data, callback);
  }

  void Read(const ReadCallback& callback) override {
    operations_->AddRead(callback);
  }

  void Delete(const WriteCallback& callback) override {
    operations_->AddDelete(callback);
  }

 private:
  OperationQueue* operations_;

  DISALLOW_COPY_AND_ASSIGN(TestDataItem);
};

class LockScreenItemStorageTest : public ExtensionsTest {
 public:
  LockScreenItemStorageTest()
      : ExtensionsTest(std::make_unique<content::TestBrowserThreadBundle>()) {}
  ~LockScreenItemStorageTest() override = default;

  void SetUp() override {
    ExtensionsTest::SetUp();

    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
    LockScreenItemStorage::RegisterLocalState(local_state_.registry());
    user_prefs::UserPrefs::Set(browser_context(), &testing_pref_service_);
    extensions_browser_client()->set_lock_screen_context(&lock_screen_context_);

    chromeos::LoginState::Initialize();
    chromeos::LoginState::Get()->SetLoggedInStateAndPrimaryUser(
        chromeos::LoginState::LOGGED_IN_ACTIVE,
        chromeos::LoginState::LOGGED_IN_USER_REGULAR, kTestUserIdHash);

    CreateTestExtension();
    item_registry_ = std::make_unique<ItemRegistry>(extension()->id());

    // Inject custom data item factory to be used with LockScreenItemStorage
    // instances.
    item_factory_ = base::Bind(&LockScreenItemStorageTest::CreateItem,
                               base::Unretained(this));
    registered_items_getter_ = base::Bind(
        &LockScreenItemStorageTest::GetRegisteredItems, base::Unretained(this));
    item_store_deleter_ = base::Bind(&LockScreenItemStorageTest::RemoveAllItems,
                                     base::Unretained(this));
    LockScreenItemStorage::SetItemProvidersForTesting(
        &registered_items_getter_, &item_factory_, &item_store_deleter_);

    ResetLockScreenItemStorage();
  }

  void TearDown() override {
    lock_screen_item_storage_.reset();
    operations_.clear();
    item_registry_.reset();
    LockScreenItemStorage::SetItemProvidersForTesting(nullptr, nullptr,
                                                      nullptr);
    chromeos::LoginState::Shutdown();
    ExtensionsTest::TearDown();
  }

  OperationQueue* GetOperations(const std::string& id) {
    return operations_[id].get();
  }

  void UnsetLockScreenItemStorage() { lock_screen_item_storage_.reset(); }

  void ResetLockScreenItemStorage() {
    lock_screen_item_storage_.reset();
    lock_screen_item_storage_ = std::make_unique<LockScreenItemStorage>(
        browser_context(), &local_state_, kTestSymmetricKey,
        test_dir_.GetPath());
  }

  // Utility method for setting test item content.
  bool SetItemContent(const std::string& id, const std::vector<char>& content) {
    OperationQueue* item_operations = GetOperations(id);
    if (!item_operations) {
      ADD_FAILURE() << "No item operations";
      return false;
    }
    OperationResult write_result = OperationResult::kFailed;
    lock_screen_item_storage()->SetItemContent(
        extension()->id(), id, content,
        base::Bind(&RecordWriteResult, &write_result));
    if (!item_operations->HasPendingOperations()) {
      ADD_FAILURE() << "Write not registered";
      return false;
    }
    item_operations->CompleteNextOperation(
        OperationQueue::OperationType::kWrite, OperationResult::kSuccess);
    EXPECT_EQ(OperationResult::kSuccess, write_result);
    return write_result == OperationResult::kSuccess;
  }

  const DataItem* CreateNewItem() {
    OperationResult create_result = OperationResult::kFailed;
    const DataItem* item = nullptr;
    lock_screen_item_storage()->CreateItem(
        extension()->id(),
        base::Bind(&RecordCreateResult, &create_result, &item));
    EXPECT_EQ(OperationResult::kSuccess, create_result);

    return item;
  }

  // Utility method for creating a new testing data item, and setting its
  // content.
  const DataItem* CreateItemWithContent(const std::vector<char>& content) {
    const DataItem* item = CreateNewItem();
    if (!item) {
      ADD_FAILURE() << "Item creation failed";
      return nullptr;
    }

    if (!SetItemContent(item->id(), content))
      return nullptr;

    return item;
  }

  void GetAllItems(std::vector<std::string>* all_items) {
    lock_screen_item_storage()->GetAllForExtension(
        extension()->id(), base::Bind(&RecordGetAllItemsResult, all_items));
  }

  // Finds an item with the ID |id| in list of items |items|.
  const DataItem* FindItem(const std::string& id,
                           const std::vector<const DataItem*> items) {
    for (const auto* item : items) {
      if (item && item->id() == id)
        return item;
    }
    return nullptr;
  }

  LockScreenItemStorage* lock_screen_item_storage() {
    return lock_screen_item_storage_.get();
  }

  content::BrowserContext* lock_screen_context() {
    return &lock_screen_context_;
  }

  const Extension* extension() const { return extension_.get(); }

  const base::FilePath& test_dir() const { return test_dir_.GetPath(); }

  PrefService* local_state() { return &local_state_; }

  ItemRegistry* item_registry() { return item_registry_.get(); }

 private:
  void CreateTestExtension() {
    DictionaryBuilder app_builder;
    app_builder.Set("background",
                    DictionaryBuilder()
                        .Set("scripts", ListBuilder().Append("script").Build())
                        .Build());
    ListBuilder app_handlers_builder;
    app_handlers_builder.Append(DictionaryBuilder()
                                    .Set("action", "new_note")
                                    .SetBoolean("enabled_on_lock_screen", true)
                                    .Build());
    extension_ =
        ExtensionBuilder()
            .SetManifest(
                DictionaryBuilder()
                    .Set("name", "Test app")
                    .Set("version", "1.0")
                    .Set("manifest_version", 2)
                    .Set("app", app_builder.Build())
                    .Set("action_handlers", app_handlers_builder.Build())
                    .Set("permissions",
                         ListBuilder().Append("lockScreen").Build())
                    .Build())
            .Build();
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension_);
  }

  // Callback for creating test data items - this is the callback passed to
  // LockScreenItemStorage via SetItemFactoryForTesting.
  std::unique_ptr<DataItem> CreateItem(const std::string& id,
                                       const std::string& extension_id,
                                       const std::string& crypto_key) {
    EXPECT_EQ(extension()->id(), extension_id);
    EXPECT_EQ(kTestSymmetricKey, crypto_key);

    // If there is an operation queue for the item id, reuse it in order to
    // retain state on LockScreenItemStorage restart.
    OperationQueue* operation_queue = GetOperations(id);
    if (!operation_queue) {
      operations_[id] =
          std::make_unique<OperationQueue>(id, item_registry_.get());
      operation_queue = operations_[id].get();
    }
    return std::make_unique<TestDataItem>(id, extension_id, crypto_key,
                                          operation_queue);
  }

  void GetRegisteredItems(const std::string& extension_id,
                          const DataItem::RegisteredValuesCallback& callback) {
    if (extension()->id() != extension_id) {
      callback.Run(OperationResult::kUnknownExtension, nullptr);
      return;
    }
    item_registry_->HandleGetRequest(callback);
  }

  void RemoveAllItems(const std::string& extension_id,
                      const base::Closure& callback) {
    ASSERT_EQ(extension()->id(), extension_id);
    item_registry_->RemoveAll();
    callback.Run();
  }

  std::unique_ptr<LockScreenItemStorage> lock_screen_item_storage_;

  content::TestBrowserContext lock_screen_context_;
  TestingPrefServiceSimple local_state_;

  base::ScopedTempDir test_dir_;

  sync_preferences::TestingPrefServiceSyncable testing_pref_service_;

  LockScreenItemStorage::ItemFactoryCallback item_factory_;
  LockScreenItemStorage::RegisteredItemsGetter registered_items_getter_;
  LockScreenItemStorage::ItemStoreDeleter item_store_deleter_;

  scoped_refptr<const Extension> extension_;

  std::unique_ptr<ItemRegistry> item_registry_;
  std::map<std::string, std::unique_ptr<OperationQueue>> operations_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenItemStorageTest);
};

}  // namespace

TEST_F(LockScreenItemStorageTest, GetDependingOnSessionState) {
  // Session state not initialized.
  EXPECT_FALSE(LockScreenItemStorage::GetIfAllowed(browser_context()));
  EXPECT_FALSE(LockScreenItemStorage::GetIfAllowed(lock_screen_context()));

  // Locked session.
  lock_screen_item_storage()->SetSessionLocked(true);
  EXPECT_FALSE(LockScreenItemStorage::GetIfAllowed(browser_context()));
  EXPECT_EQ(lock_screen_item_storage(),
            LockScreenItemStorage::GetIfAllowed(lock_screen_context()));

  lock_screen_item_storage()->SetSessionLocked(false);

  EXPECT_EQ(lock_screen_item_storage(),
            LockScreenItemStorage::GetIfAllowed(browser_context()));
  EXPECT_FALSE(LockScreenItemStorage::GetIfAllowed(lock_screen_context()));
}

TEST_F(LockScreenItemStorageTest, SetAndGetContent) {
  lock_screen_item_storage()->SetSessionLocked(true);

  const DataItem* item = CreateNewItem();
  ASSERT_TRUE(item);

  std::vector<std::string> all_items;
  GetAllItems(&all_items);
  ASSERT_EQ(1u, all_items.size());
  EXPECT_EQ(item->id(), all_items[0]);

  OperationQueue* item_operations = GetOperations(item->id());
  ASSERT_TRUE(item_operations);
  EXPECT_FALSE(item_operations->HasPendingOperations());

  std::vector<char> content = {'f', 'i', 'l', 'e'};
  OperationResult write_result = OperationResult::kFailed;
  lock_screen_item_storage()->SetItemContent(
      extension()->id(), item->id(), content,
      base::Bind(&RecordWriteResult, &write_result));

  item_operations->CompleteNextOperation(OperationQueue::OperationType::kWrite,
                                         OperationResult::kSuccess);

  EXPECT_EQ(OperationResult::kSuccess, write_result);
  EXPECT_EQ(content, item_operations->content());

  OperationResult read_result = OperationResult::kFailed;
  std::unique_ptr<std::vector<char>> read_content;

  lock_screen_item_storage()->GetItemContent(
      extension()->id(), item->id(),
      base::Bind(&RecordReadResult, &read_result, &read_content));

  item_operations->CompleteNextOperation(OperationQueue::OperationType::kRead,
                                         OperationResult::kSuccess);
  EXPECT_EQ(OperationResult::kSuccess, read_result);
  EXPECT_EQ(content, *read_content);

  OperationResult delete_result = OperationResult::kFailed;
  lock_screen_item_storage()->DeleteItem(
      extension()->id(), item->id(),
      base::Bind(&RecordWriteResult, &delete_result));

  item_operations->CompleteNextOperation(OperationQueue::OperationType::kDelete,
                                         OperationResult::kSuccess);
  EXPECT_EQ(OperationResult::kSuccess, delete_result);
  EXPECT_TRUE(item_operations->deleted());
}

TEST_F(LockScreenItemStorageTest, FailToInitializeData) {
  lock_screen_item_storage()->SetSessionLocked(true);

  const DataItem* item = CreateNewItem();
  ASSERT_TRUE(item);
  const std::string item_id = item->id();

  ResetLockScreenItemStorage();
  item_registry()->set_fail(true);

  OperationResult write_result = OperationResult::kFailed;
  lock_screen_item_storage()->SetItemContent(
      extension()->id(), item_id, {'x'},
      base::Bind(&RecordWriteResult, &write_result));
  EXPECT_EQ(OperationResult::kNotFound, write_result);

  OperationResult read_result = OperationResult::kFailed;
  std::unique_ptr<std::vector<char>> read_content;
  lock_screen_item_storage()->GetItemContent(
      extension()->id(), item_id,
      base::Bind(&RecordReadResult, &read_result, &read_content));
  EXPECT_EQ(OperationResult::kNotFound, read_result);

  OperationResult delete_result = OperationResult::kFailed;
  lock_screen_item_storage()->DeleteItem(
      extension()->id(), "non_existen",
      base::Bind(&RecordWriteResult, &delete_result));
  EXPECT_EQ(OperationResult::kNotFound, delete_result);

  OperationQueue* operations = GetOperations(item_id);
  ASSERT_TRUE(operations);
  EXPECT_FALSE(operations->HasPendingOperations());

  item_registry()->set_fail(false);

  const DataItem* new_item = CreateNewItem();
  ASSERT_TRUE(new_item);

  write_result = OperationResult::kFailed;
  lock_screen_item_storage()->SetItemContent(
      extension()->id(), new_item->id(), {'y'},
      base::Bind(&RecordWriteResult, &write_result));

  OperationQueue* new_item_operations = GetOperations(new_item->id());
  ASSERT_TRUE(new_item_operations);
  new_item_operations->CompleteNextOperation(
      OperationQueue::OperationType::kWrite, OperationResult::kSuccess);
  EXPECT_EQ(OperationResult::kSuccess, write_result);

  std::vector<std::string> items;
  GetAllItems(&items);
  ASSERT_EQ(1u, items.size());
  EXPECT_EQ(new_item->id(), items[0]);
}

TEST_F(LockScreenItemStorageTest, RequestsDuringInitialLoad) {
  lock_screen_item_storage()->SetSessionLocked(true);

  const DataItem* item = CreateNewItem();
  ASSERT_TRUE(item);
  const std::string item_id = item->id();

  item_registry()->set_throttle_get(true);
  ResetLockScreenItemStorage();

  EXPECT_FALSE(item_registry()->HasPendingCallback());

  OperationResult write_result = OperationResult::kFailed;
  lock_screen_item_storage()->SetItemContent(
      extension()->id(), item_id, {'x'},
      base::Bind(&RecordWriteResult, &write_result));

  OperationResult read_result = OperationResult::kFailed;
  std::unique_ptr<std::vector<char>> read_content;
  lock_screen_item_storage()->GetItemContent(
      extension()->id(), item_id,
      base::Bind(&RecordReadResult, &read_result, &read_content));

  std::vector<std::string> items;
  lock_screen_item_storage()->GetAllForExtension(
      extension()->id(), base::Bind(&RecordGetAllItemsResult, &items));
  EXPECT_TRUE(items.empty());

  OperationResult delete_result = OperationResult::kFailed;
  lock_screen_item_storage()->DeleteItem(
      extension()->id(), item_id,
      base::Bind(&RecordWriteResult, &delete_result));

  OperationQueue* operations = GetOperations(item_id);
  ASSERT_TRUE(operations);
  EXPECT_FALSE(operations->HasPendingOperations());

  OperationResult create_result = OperationResult::kFailed;
  const DataItem* new_item = nullptr;
  lock_screen_item_storage()->CreateItem(
      extension()->id(),
      base::Bind(&RecordCreateResult, &create_result, &new_item));
  EXPECT_FALSE(new_item);

  EXPECT_TRUE(item_registry()->HasPendingCallback());
  item_registry()->RunPendingCallback();

  EXPECT_TRUE(operations->HasPendingOperations());

  operations->CompleteNextOperation(OperationQueue::OperationType::kWrite,
                                    OperationResult::kSuccess);
  operations->CompleteNextOperation(OperationQueue::OperationType::kRead,
                                    OperationResult::kSuccess);
  operations->CompleteNextOperation(OperationQueue::OperationType::kDelete,
                                    OperationResult::kSuccess);

  EXPECT_EQ(OperationResult::kSuccess, write_result);
  EXPECT_EQ(OperationResult::kSuccess, read_result);
  ASSERT_TRUE(read_content);
  EXPECT_EQ(std::vector<char>({'x'}), *read_content);
  EXPECT_EQ(OperationResult::kSuccess, delete_result);
  EXPECT_EQ(OperationResult::kSuccess, create_result);

  EXPECT_TRUE(new_item);

  ASSERT_EQ(1u, items.size());
  EXPECT_EQ(item_id, items[0]);

  GetAllItems(&items);
  ASSERT_EQ(1u, items.size());
  EXPECT_EQ(new_item->id(), items[0]);
}

TEST_F(LockScreenItemStorageTest, HandleNonExistent) {
  lock_screen_item_storage()->SetSessionLocked(true);

  const DataItem* item = CreateNewItem();
  ASSERT_TRUE(item);

  std::vector<char> content = {'x'};

  OperationResult write_result = OperationResult::kFailed;
  lock_screen_item_storage()->SetItemContent(
      extension()->id(), "non_existent", content,
      base::Bind(&RecordWriteResult, &write_result));
  EXPECT_EQ(OperationResult::kNotFound, write_result);

  write_result = OperationResult::kFailed;
  lock_screen_item_storage()->SetItemContent(
      "non_existent", item->id(), content,
      base::Bind(&RecordWriteResult, &write_result));
  EXPECT_EQ(OperationResult::kNotFound, write_result);

  OperationResult read_result = OperationResult::kFailed;
  std::unique_ptr<std::vector<char>> read_content;
  lock_screen_item_storage()->GetItemContent(
      extension()->id(), "non_existent",
      base::Bind(&RecordReadResult, &read_result, &read_content));
  EXPECT_EQ(OperationResult::kNotFound, read_result);
  read_result = OperationResult::kFailed;

  lock_screen_item_storage()->GetItemContent(
      "non_existent", item->id(),
      base::Bind(&RecordReadResult, &read_result, &read_content));
  EXPECT_EQ(OperationResult::kNotFound, read_result);

  OperationResult delete_result = OperationResult::kFailed;
  lock_screen_item_storage()->DeleteItem(
      extension()->id(), "non_existen",
      base::Bind(&RecordWriteResult, &delete_result));
  EXPECT_EQ(OperationResult::kNotFound, delete_result);

  delete_result = OperationResult::kFailed;
  lock_screen_item_storage()->DeleteItem(
      "non_existent", item->id(),
      base::Bind(&RecordWriteResult, &delete_result));
  EXPECT_EQ(OperationResult::kNotFound, delete_result);
}

TEST_F(LockScreenItemStorageTest, HandleFailure) {
  lock_screen_item_storage()->SetSessionLocked(true);

  const DataItem* item = CreateItemWithContent({'x'});
  ASSERT_TRUE(item);
  OperationQueue* operations = GetOperations(item->id());
  ASSERT_TRUE(operations);

  OperationResult write_result = OperationResult::kFailed;
  lock_screen_item_storage()->SetItemContent(
      extension()->id(), item->id(), {'x'},
      base::Bind(&RecordWriteResult, &write_result));
  operations->CompleteNextOperation(OperationQueue::OperationType::kWrite,
                                    OperationResult::kInvalidKey);
  EXPECT_EQ(OperationResult::kInvalidKey, write_result);

  OperationResult read_result = OperationResult::kFailed;
  std::unique_ptr<std::vector<char>> read_content;
  lock_screen_item_storage()->GetItemContent(
      extension()->id(), item->id(),
      base::Bind(&RecordReadResult, &read_result, &read_content));
  operations->CompleteNextOperation(OperationQueue::OperationType::kRead,
                                    OperationResult::kWrongKey);
  EXPECT_EQ(OperationResult::kWrongKey, read_result);
  EXPECT_FALSE(read_content);

  EXPECT_FALSE(operations->HasPendingOperations());
}

TEST_F(LockScreenItemStorageTest, DataItemsAvailableEventOnUnlock) {
  TestEventRouter* event_router = static_cast<TestEventRouter*>(
      extensions::EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          browser_context(), &TestEventRouterFactoryFunction));
  ASSERT_TRUE(event_router);

  EXPECT_TRUE(event_router->was_locked_values().empty());

  lock_screen_item_storage()->SetSessionLocked(true);
  EXPECT_TRUE(event_router->was_locked_values().empty());

  // No event since no data items associated with the app exist.
  lock_screen_item_storage()->SetSessionLocked(false);
  EXPECT_TRUE(event_router->was_locked_values().empty());

  lock_screen_item_storage()->SetSessionLocked(true);
  const DataItem* item = CreateItemWithContent({'f', 'i', 'l', 'e', '1'});
  const std::string item_id = item->id();
  EXPECT_TRUE(event_router->was_locked_values().empty());

  // There's an available data item, so unlock should trigger the event.
  lock_screen_item_storage()->SetSessionLocked(false);
  EXPECT_EQ(std::vector<bool>({true}), event_router->was_locked_values());
  event_router->ClearWasLockedValues();

  // Update the item content while the session is unlocked.
  EXPECT_TRUE(SetItemContent(item_id, {'f', 'i', 'l', 'e', '2'}));

  lock_screen_item_storage()->SetSessionLocked(true);

  // Data item is still around - notify the app it's available.
  lock_screen_item_storage()->SetSessionLocked(false);
  EXPECT_EQ(std::vector<bool>({true}), event_router->was_locked_values());
  event_router->ClearWasLockedValues();

  lock_screen_item_storage()->SetSessionLocked(true);

  EXPECT_TRUE(SetItemContent(item_id, {'f', 'i', 'l', 'e', '3'}));

  lock_screen_item_storage()->SetSessionLocked(false);
  EXPECT_EQ(std::vector<bool>({true}), event_router->was_locked_values());
  event_router->ClearWasLockedValues();

  // When the item is deleted, the data item avilable event should stop firing.
  OperationResult delete_result;
  lock_screen_item_storage()->DeleteItem(
      extension()->id(), item_id,
      base::Bind(&RecordWriteResult, &delete_result));
  OperationQueue* operations = GetOperations(item_id);
  ASSERT_TRUE(operations);
  operations->CompleteNextOperation(OperationQueue::OperationType::kDelete,
                                    OperationResult::kSuccess);
  lock_screen_item_storage()->SetSessionLocked(false);
  lock_screen_item_storage()->SetSessionLocked(true);

  EXPECT_TRUE(event_router->was_locked_values().empty());
}

TEST_F(LockScreenItemStorageTest,
       NoDataItemsAvailableEventAfterFailedCreation) {
  TestEventRouter* event_router = static_cast<TestEventRouter*>(
      extensions::EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          browser_context(), &TestEventRouterFactoryFunction));
  ASSERT_TRUE(event_router);

  lock_screen_item_storage()->SetSessionLocked(true);

  item_registry()->set_allow_new(false);

  OperationResult create_result = OperationResult::kFailed;
  const DataItem* item = nullptr;
  lock_screen_item_storage()->CreateItem(
      extension()->id(),
      base::Bind(&RecordCreateResult, &create_result, &item));
  EXPECT_EQ(OperationResult::kFailed, create_result);

  lock_screen_item_storage()->SetSessionLocked(false);
  lock_screen_item_storage()->SetSessionLocked(true);
  EXPECT_TRUE(event_router->was_locked_values().empty());
}

TEST_F(LockScreenItemStorageTest, DataItemsAvailableEventOnRestart) {
  TestEventRouter* event_router = static_cast<TestEventRouter*>(
      extensions::EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          browser_context(), &TestEventRouterFactoryFunction));
  ASSERT_TRUE(event_router);

  EXPECT_TRUE(event_router->was_locked_values().empty());

  lock_screen_item_storage()->SetSessionLocked(true);
  EXPECT_TRUE(event_router->was_locked_values().empty());

  const DataItem* item = CreateItemWithContent({'f', 'i', 'l', 'e', '1'});
  EXPECT_TRUE(event_router->was_locked_values().empty());
  const std::string item_id = item->id();

  ResetLockScreenItemStorage();

  EXPECT_TRUE(event_router->was_locked_values().empty());
  lock_screen_item_storage()->SetSessionLocked(false);

  EXPECT_EQ(std::vector<bool>({false}), event_router->was_locked_values());
  event_router->ClearWasLockedValues();

  // The event should be dispatched on next unlock event, as long as a valid
  // item exists.
  ResetLockScreenItemStorage();
  lock_screen_item_storage()->SetSessionLocked(false);

  EXPECT_EQ(std::vector<bool>({false}), event_router->was_locked_values());
  event_router->ClearWasLockedValues();

  ResetLockScreenItemStorage();

  OperationResult delete_result = OperationResult::kFailed;
  lock_screen_item_storage()->DeleteItem(
      extension()->id(), item_id,
      base::Bind(&RecordWriteResult, &delete_result));
  OperationQueue* operations = GetOperations(item_id);
  ASSERT_TRUE(operations);
  operations->CompleteNextOperation(OperationQueue::OperationType::kDelete,
                                    OperationResult::kSuccess);

  lock_screen_item_storage()->SetSessionLocked(false);
  EXPECT_TRUE(event_router->was_locked_values().empty());
}

TEST_F(LockScreenItemStorageTest, ClearDataOnUninstall) {
  const DataItem* item = CreateItemWithContent({'x'});
  ASSERT_TRUE(item);

  ExtensionRegistry::Get(browser_context())->RemoveEnabled(extension()->id());
  ExtensionRegistry::Get(browser_context())
      ->TriggerOnUninstalled(extension(), UNINSTALL_REASON_FOR_TESTING);
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension());

  std::vector<std::string> items;
  GetAllItems(&items);
  EXPECT_TRUE(items.empty());
}

TEST_F(LockScreenItemStorageTest,
       ClearOnUninstallWhileLockScreenItemStorageNotSet) {
  TestEventRouter* event_router = static_cast<TestEventRouter*>(
      extensions::EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          browser_context(), &TestEventRouterFactoryFunction));
  ASSERT_TRUE(event_router);

  const DataItem* item = CreateItemWithContent({'x'});
  ASSERT_TRUE(item);

  UnsetLockScreenItemStorage();

  ExtensionRegistry::Get(browser_context())->RemoveEnabled(extension()->id());
  ExtensionRegistry::Get(browser_context())
      ->TriggerOnUninstalled(extension(), UNINSTALL_REASON_FOR_TESTING);

  ResetLockScreenItemStorage();
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension());
  lock_screen_item_storage()->SetSessionLocked(false);

  std::vector<std::string> items;
  GetAllItems(&items);
  EXPECT_TRUE(items.empty());

  EXPECT_TRUE(event_router->was_locked_values().empty());
}

}  // namespace lock_screen_data
}  // namespace extensions
