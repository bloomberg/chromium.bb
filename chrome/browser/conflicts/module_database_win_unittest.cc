// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_database_win.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A simple mechanism for running a single task on a separate thread.
class SingleTaskRunner : public base::SimpleThread {
 public:
  explicit SingleTaskRunner(base::Closure task)
      : base::SimpleThread("SingleTaskRunner"), task_(std::move(task)) {}

  // Runs the provided task and exits.
  void Run() override { task_.Run(); }

 private:
  base::Closure task_;
  DISALLOW_COPY_AND_ASSIGN(SingleTaskRunner);
};

// Launches a thread and runs a single task on it.
void RunTask(base::Closure task) {
  SingleTaskRunner task_runner(std::move(task));
  task_runner.Start();
  task_runner.Join();
}

constexpr uint32_t kPid1 = 1234u;
constexpr uint32_t kPid2 = 2345u;

constexpr uint64_t kCreateTime1 = 1234u;
constexpr uint64_t kCreateTime2 = 2345u;

constexpr wchar_t kDll1[] = L"dummy.dll";
constexpr wchar_t kDll2[] = L"foo.dll";

constexpr size_t kSize1 = 100 * 4096;
constexpr size_t kSize2 = 20 * 4096;

constexpr uint32_t kTime1 = 0xDEADBEEF;
constexpr uint32_t kTime2 = 0xBAADF00D;

constexpr uintptr_t kGoodAddress1 = 0x04000000u;
constexpr uintptr_t kGoodAddress2 = 0x05000000u;

}  // namespace

class TestModuleDatabase : ModuleDatabase {
 public:
  // Types.
  using ModuleDatabase::ModuleLoadAddresses;

  // Constants.
  using ModuleDatabase::kInvalidIndex;

  // Functions.
  using ModuleDatabase::FindLoadAddressIndexById;
  using ModuleDatabase::FindLoadAddressIndexByAddress;
  using ModuleDatabase::InsertLoadAddress;
  using ModuleDatabase::RemoveLoadAddressById;
  using ModuleDatabase::RemoveLoadAddressByIndex;
};

class ModuleDatabaseTest : public testing::Test {
 protected:
  ModuleDatabaseTest()
      : dll1_(kDll1),
        dll2_(kDll2),
        message_loop_(base::MakeUnique<base::MessageLoop>()),
        module_database_(
            base::MakeUnique<ModuleDatabase>(message_loop_->task_runner())) {}

  void RunLoopUntilIdle() { base::RunLoop().RunUntilIdle(); }

  const ModuleDatabase::ModuleMap& modules() {
    return module_database_->modules_;
  }

  const ModuleDatabase::ProcessMap& processes() {
    return module_database_->processes_;
  }

  static uint32_t ProcessTypeToBit(content::ProcessType process_type) {
    return ModuleDatabase::ProcessTypeToBit(process_type);
  }

  // Counts the occurrences of the given |module_id| in the given collection of
  // |load_addresses|.
  static size_t ModuleIdCount(
      ModuleId module_id,
      const ModuleDatabase::ModuleLoadAddresses& load_addresses) {
    return std::count_if(
        load_addresses.begin(), load_addresses.end(),
        [module_id](const auto& x) { return module_id == x.first; });
  }

 protected:
  const base::FilePath dll1_;
  const base::FilePath dll2_;

  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<ModuleDatabase> module_database_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ModuleDatabaseTest);
};

TEST_F(ModuleDatabaseTest, LoadAddressVectorOperations) {
  using TMD = TestModuleDatabase;
  TestModuleDatabase::ModuleLoadAddresses la;

  // Finds should fail in an empty collection.
  EXPECT_EQ(TMD::kInvalidIndex, TMD::FindLoadAddressIndexById(0, la));
  EXPECT_EQ(TMD::kInvalidIndex, TMD::FindLoadAddressIndexById(0x04000000, la));

  // A first insert should work. Don't start with ModuleId 0 so that later
  // inserts can insert that module.
  TMD::InsertLoadAddress(10, 0x04000000, &la);
  EXPECT_EQ(1u, la.size());
  EXPECT_EQ(10, la[0].first);
  EXPECT_EQ(0x04000000u, la[0].second);

  // Finds should work.
  EXPECT_EQ(TMD::kInvalidIndex, TMD::FindLoadAddressIndexById(0, la));
  EXPECT_EQ(TMD::kInvalidIndex, TMD::FindLoadAddressIndexById(0x03000000, la));
  EXPECT_EQ(0u, TMD::FindLoadAddressIndexById(10, la));
  EXPECT_EQ(0u, TMD::FindLoadAddressIndexByAddress(0x04000000, la));

  // A second insert should work. This is the new max so should be at the end
  // of the collection.
  TMD::InsertLoadAddress(12, 0x06000000, &la);
  EXPECT_EQ(2u, la.size());
  EXPECT_EQ(10, la[0].first);
  EXPECT_EQ(0x04000000u, la[0].second);
  EXPECT_EQ(12, la[1].first);
  EXPECT_EQ(0x06000000u, la[1].second);

  // Finds should work.
  EXPECT_EQ(TMD::kInvalidIndex, TMD::FindLoadAddressIndexById(0, la));
  EXPECT_EQ(TMD::kInvalidIndex, TMD::FindLoadAddressIndexById(0x03000000, la));
  EXPECT_EQ(0u, TMD::FindLoadAddressIndexById(10, la));
  EXPECT_EQ(0u, TMD::FindLoadAddressIndexByAddress(0x04000000, la));
  EXPECT_EQ(1u, TMD::FindLoadAddressIndexById(12, la));
  EXPECT_EQ(1u, TMD::FindLoadAddressIndexByAddress(0x06000000, la));

  // Another insert should work. This is not the new max, so a swap should
  // happen to keep the maximum element at the end of the collection.
  TMD::InsertLoadAddress(11, 0x05000000, &la);
  EXPECT_EQ(3u, la.size());
  EXPECT_EQ(10, la[0].first);
  EXPECT_EQ(0x04000000u, la[0].second);
  EXPECT_EQ(11, la[1].first);
  EXPECT_EQ(0x05000000u, la[1].second);
  EXPECT_EQ(12, la[2].first);
  EXPECT_EQ(0x06000000u, la[2].second);

  // An insert of an existing module should work, but simply overwrite the
  // load address.
  TMD::InsertLoadAddress(11, 0x0F000000, &la);
  EXPECT_EQ(3u, la.size());
  EXPECT_EQ(11, la[1].first);
  EXPECT_EQ(0x0F000000u, la[1].second);
  TMD::InsertLoadAddress(11, 0x05000000, &la);
  EXPECT_EQ(3u, la.size());
  EXPECT_EQ(11, la[1].first);
  EXPECT_EQ(0x05000000u, la[1].second);

  // Finds should work.
  EXPECT_EQ(TMD::kInvalidIndex, TMD::FindLoadAddressIndexById(0, la));
  EXPECT_EQ(TMD::kInvalidIndex, TMD::FindLoadAddressIndexById(0x03000000, la));
  EXPECT_EQ(0u, TMD::FindLoadAddressIndexById(10, la));
  EXPECT_EQ(0u, TMD::FindLoadAddressIndexByAddress(0x04000000, la));
  EXPECT_EQ(1u, TMD::FindLoadAddressIndexById(11, la));
  EXPECT_EQ(1u, TMD::FindLoadAddressIndexByAddress(0x05000000, la));
  EXPECT_EQ(2u, TMD::FindLoadAddressIndexById(12, la));
  EXPECT_EQ(2u, TMD::FindLoadAddressIndexByAddress(0x06000000, la));

  // Do some inserts of lower modules IDs. This ensures that we'll have some
  // higher module IDs in the vector before some lower modules IDs, for testing
  // the deletion logic.
  TMD::InsertLoadAddress(3, 0x07000000, &la);
  TMD::InsertLoadAddress(4, 0x08000000, &la);
  TMD::InsertLoadAddress(5, 0x09000000, &la);
  EXPECT_EQ(6u, la.size());
  EXPECT_EQ(10, la[0].first);
  EXPECT_EQ(0x04000000u, la[0].second);
  EXPECT_EQ(11, la[1].first);
  EXPECT_EQ(0x05000000u, la[1].second);
  EXPECT_EQ(3, la[2].first);
  EXPECT_EQ(0x07000000u, la[2].second);
  EXPECT_EQ(4, la[3].first);
  EXPECT_EQ(0x08000000u, la[3].second);
  EXPECT_EQ(5, la[4].first);
  EXPECT_EQ(0x09000000u, la[4].second);
  EXPECT_EQ(12, la[5].first);
  EXPECT_EQ(0x06000000u, la[5].second);

  // Remove an element that isn't in the second last position. The second last
  // element should be swapped into its position, and the last element moved
  // to the second last place.
  TMD::RemoveLoadAddressByIndex(2, &la);
  EXPECT_EQ(5u, la.size());
  EXPECT_EQ(10, la[0].first);
  EXPECT_EQ(0x04000000u, la[0].second);
  EXPECT_EQ(11, la[1].first);
  EXPECT_EQ(0x05000000u, la[1].second);
  EXPECT_EQ(5, la[2].first);
  EXPECT_EQ(0x09000000u, la[2].second);
  EXPECT_EQ(4, la[3].first);
  EXPECT_EQ(0x08000000u, la[3].second);
  EXPECT_EQ(12, la[4].first);
  EXPECT_EQ(0x06000000u, la[4].second);

  // Remove the second last element. Only the last element should move.
  TMD::RemoveLoadAddressByIndex(3, &la);
  EXPECT_EQ(4u, la.size());
  EXPECT_EQ(10, la[0].first);
  EXPECT_EQ(0x04000000u, la[0].second);
  EXPECT_EQ(11, la[1].first);
  EXPECT_EQ(0x05000000u, la[1].second);
  EXPECT_EQ(5, la[2].first);
  EXPECT_EQ(0x09000000u, la[2].second);
  EXPECT_EQ(12, la[3].first);
  EXPECT_EQ(0x06000000u, la[3].second);

  // Remove the last element. The new maximum should be found moved to the
  // end.
  TMD::RemoveLoadAddressByIndex(3, &la);
  EXPECT_EQ(3u, la.size());
  EXPECT_EQ(10, la[0].first);
  EXPECT_EQ(0x04000000u, la[0].second);
  EXPECT_EQ(5, la[1].first);
  EXPECT_EQ(0x09000000u, la[1].second);
  EXPECT_EQ(11, la[2].first);
  EXPECT_EQ(0x05000000u, la[2].second);

  // Remove the last element by ModuleId. The remaining modules should be
  // swapped.
  TMD::RemoveLoadAddressById(11, &la);
  EXPECT_EQ(2u, la.size());
  EXPECT_EQ(5, la[0].first);
  EXPECT_EQ(0x09000000u, la[0].second);
  EXPECT_EQ(10, la[1].first);
  EXPECT_EQ(0x04000000u, la[1].second);

  // Remove the first element by ModuleId.
  TMD::RemoveLoadAddressById(5, &la);
  EXPECT_EQ(1u, la.size());
  EXPECT_EQ(10, la[0].first);
  EXPECT_EQ(0x04000000u, la[0].second);

  // Remove the only remaining element.
  TMD::RemoveLoadAddressByIndex(0, &la);
  EXPECT_TRUE(la.empty());
}

TEST_F(ModuleDatabaseTest, LoadAddressVectorStressTest) {
  using TMD = TestModuleDatabase;
  TestModuleDatabase::ModuleLoadAddresses la;

  for (size_t n = 1; n < 200; ++n) {
    // Will keep track of which elements have been inserted.
    std::vector<bool> inserted(n);
    size_t inserted_count = 0;

    // Generate a shuffled list of IDs. This will be the insertion order.
    // More insertions than elements will occur so that rewrites occur.,
    std::vector<ModuleId> ids(11 * n / 10);
    for (size_t i = 0; i < 11 * n / 10; ++i)
      ids[i] = i % n;
    std::random_shuffle(ids.begin(), ids.end());

    // Do the insertions.
    for (auto id : ids) {
      if (!inserted[id]) {
        inserted[id] = true;
        ++inserted_count;
      }

      // Generate a load address. The load address bakes in the index so that
      // searching by load address is easy.
      uintptr_t load_address = static_cast<uintptr_t>(id) << 16;

      // Do the insertion.
      TMD::InsertLoadAddress(id, load_address, &la);
      EXPECT_EQ(inserted_count, la.size());
    }

    // Validate that every element is there, via both search mechanisms.
    for (size_t id = 0; id < n; ++id) {
      uintptr_t load_address = static_cast<uintptr_t>(id) << 16;
      size_t index1 = TMD::FindLoadAddressIndexById(id, la);
      size_t index2 = TMD::FindLoadAddressIndexByAddress(load_address, la);
      EXPECT_NE(TMD::kInvalidIndex, index1);
      EXPECT_EQ(index1, index2);
    }

    // Generate the deletion order.
    ids.resize(n);
    for (size_t i = 0; i < ids.size(); ++i)
      ids[i] = i;
    std::random_shuffle(ids.begin(), ids.end());

    // Do the deletions.
    for (auto id : ids) {
      --inserted_count;
      TMD::RemoveLoadAddressById(id, &la);
      EXPECT_EQ(inserted_count, la.size());
    }
  }
}

TEST_F(ModuleDatabaseTest, TasksAreBounced) {
  // Run a task on the current thread. This should not be bounced, so no
  // task should be scheduled on the task runner.
  module_database_->OnProcessStarted(kPid1, kCreateTime1,
                                     content::PROCESS_TYPE_BROWSER);
  EXPECT_TRUE(message_loop_->IsIdleForTesting());
  module_database_->OnModuleLoad(kPid1, kCreateTime1, dll1_, kSize1, kTime1,
                                 kGoodAddress1);
  EXPECT_TRUE(message_loop_->IsIdleForTesting());
  module_database_->OnProcessEnded(kPid1, kCreateTime1);
  EXPECT_TRUE(message_loop_->IsIdleForTesting());

  // Indicate another process start on this thread. This call can't be
  // bounced.
  module_database_->OnProcessStarted(kPid2, kCreateTime2,
                                     content::PROCESS_TYPE_BROWSER);

  // Run similar tasks on another thread. These should be bounced.
  RunTask(base::Bind(&ModuleDatabase::OnModuleLoad,
                     base::Unretained(module_database_.get()), kPid2,
                     kCreateTime2, dll1_, kSize1, kTime1, kGoodAddress1));
  EXPECT_FALSE(message_loop_->IsIdleForTesting());
  RunLoopUntilIdle();

  RunTask(base::Bind(&ModuleDatabase::OnProcessEnded,
                     base::Unretained(module_database_.get()), kPid2,
                     kCreateTime2));
  EXPECT_FALSE(message_loop_->IsIdleForTesting());
  RunLoopUntilIdle();
}

TEST_F(ModuleDatabaseTest, EventsWithoutProcessIgnore) {
  EXPECT_EQ(0u, modules().size());
  EXPECT_EQ(0u, processes().size());

  module_database_->OnModuleLoad(kPid1, kCreateTime1, dll1_, kSize1, kTime1,
                                 kGoodAddress1);

  EXPECT_EQ(0u, modules().size());
  EXPECT_EQ(0u, processes().size());
}

TEST_F(ModuleDatabaseTest, OrphanedUnloadIgnored) {
  EXPECT_EQ(0u, modules().size());
  EXPECT_EQ(0u, processes().size());

  // Start a process.
  module_database_->OnProcessStarted(kPid1, kCreateTime1,
                                     content::PROCESS_TYPE_BROWSER);
  EXPECT_EQ(0u, modules().size());
  EXPECT_EQ(1u, processes().size());
  auto p1 = processes().begin();
  EXPECT_EQ(kPid1, p1->first.process_id);
  EXPECT_EQ(kCreateTime1, p1->first.creation_time);
  EXPECT_EQ(content::PROCESS_TYPE_BROWSER, p1->first.process_type);
  EXPECT_EQ(0u, p1->second.loaded_modules.size());
  EXPECT_EQ(0u, p1->second.unloaded_modules.size());

  // Indicate a module unload. This should do nothing because there's no
  // corresponding module.
  module_database_->OnModuleUnload(kPid1, kCreateTime1, kGoodAddress1);
  EXPECT_EQ(0u, modules().size());
  EXPECT_EQ(1u, processes().size());
  EXPECT_EQ(0u, p1->second.loaded_modules.size());
  EXPECT_EQ(0u, p1->second.unloaded_modules.size());
}

TEST_F(ModuleDatabaseTest, DatabaseIsConsistent) {
  EXPECT_EQ(0u, modules().size());
  EXPECT_EQ(0u, processes().size());

  // Start a process.
  module_database_->OnProcessStarted(kPid1, kCreateTime1,
                                     content::PROCESS_TYPE_BROWSER);
  EXPECT_EQ(0u, modules().size());
  EXPECT_EQ(1u, processes().size());
  auto p1 = processes().begin();
  EXPECT_EQ(kPid1, p1->first.process_id);
  EXPECT_EQ(kCreateTime1, p1->first.creation_time);
  EXPECT_EQ(content::PROCESS_TYPE_BROWSER, p1->first.process_type);
  EXPECT_EQ(0u, p1->second.loaded_modules.size());
  EXPECT_EQ(0u, p1->second.unloaded_modules.size());

  // Load a module.
  module_database_->OnModuleLoad(kPid1, kCreateTime1, dll1_, kSize1, kTime1,
                                 kGoodAddress1);
  EXPECT_EQ(1u, modules().size());
  EXPECT_EQ(1u, processes().size());

  // Ensure that the process and module sets are up to date.
  auto m1 = modules().begin();
  EXPECT_EQ(dll1_, m1->first.module_path);
  EXPECT_EQ(ProcessTypeToBit(content::PROCESS_TYPE_BROWSER),
            m1->second.process_types);
  EXPECT_EQ(kPid1, p1->first.process_id);
  EXPECT_EQ(kCreateTime1, p1->first.creation_time);
  EXPECT_EQ(content::PROCESS_TYPE_BROWSER, p1->first.process_type);
  EXPECT_EQ(1u, p1->second.loaded_modules.size());
  EXPECT_EQ(0u, p1->second.unloaded_modules.size());
  EXPECT_EQ(1u, ModuleIdCount(m1->first.module_id, p1->second.loaded_modules));

  // Provide a redundant load message for that module.
  module_database_->OnModuleLoad(kPid1, kCreateTime1, dll1_, kSize1, kTime1,
                                 kGoodAddress1);
  EXPECT_EQ(1u, modules().size());
  EXPECT_EQ(1u, processes().size());

  // Ensure that the process and module sets haven't changed.
  EXPECT_EQ(dll1_, m1->first.module_path);
  EXPECT_EQ(ProcessTypeToBit(content::PROCESS_TYPE_BROWSER),
            m1->second.process_types);
  EXPECT_EQ(kPid1, p1->first.process_id);
  EXPECT_EQ(kCreateTime1, p1->first.creation_time);
  EXPECT_EQ(content::PROCESS_TYPE_BROWSER, p1->first.process_type);
  EXPECT_EQ(1u, p1->second.loaded_modules.size());
  EXPECT_EQ(0u, p1->second.unloaded_modules.size());
  EXPECT_EQ(1u, ModuleIdCount(m1->first.module_id, p1->second.loaded_modules));

  // Load a second module into the process.
  module_database_->OnModuleLoad(kPid1, kCreateTime1, dll2_, kSize2, kTime2,
                                 kGoodAddress2);
  EXPECT_EQ(2u, modules().size());
  EXPECT_EQ(1u, processes().size());

  // Ensure that the process and module sets are up to date.
  auto m2 = modules().rbegin();
  EXPECT_EQ(dll2_, m2->first.module_path);
  EXPECT_EQ(ProcessTypeToBit(content::PROCESS_TYPE_BROWSER),
            m2->second.process_types);
  EXPECT_EQ(kPid1, p1->first.process_id);
  EXPECT_EQ(kCreateTime1, p1->first.creation_time);
  EXPECT_EQ(content::PROCESS_TYPE_BROWSER, p1->first.process_type);
  EXPECT_EQ(2u, p1->second.loaded_modules.size());
  EXPECT_EQ(0u, p1->second.unloaded_modules.size());
  EXPECT_EQ(1u, ModuleIdCount(m1->first.module_id, p1->second.loaded_modules));
  EXPECT_EQ(1u, ModuleIdCount(m2->first.module_id, p1->second.loaded_modules));

  // Unload the second module.
  module_database_->OnModuleUnload(kPid1, kCreateTime1, kGoodAddress2);
  EXPECT_EQ(2u, modules().size());
  EXPECT_EQ(1u, processes().size());

  // Ensure that the process and module sets are up to date.
  EXPECT_EQ(dll2_, m2->first.module_path);
  EXPECT_EQ(ProcessTypeToBit(content::PROCESS_TYPE_BROWSER),
            m2->second.process_types);
  EXPECT_EQ(kPid1, p1->first.process_id);
  EXPECT_EQ(kCreateTime1, p1->first.creation_time);
  EXPECT_EQ(content::PROCESS_TYPE_BROWSER, p1->first.process_type);
  EXPECT_EQ(1u, p1->second.loaded_modules.size());
  EXPECT_EQ(1u, p1->second.unloaded_modules.size());
  EXPECT_EQ(1u, ModuleIdCount(m1->first.module_id, p1->second.loaded_modules));
  EXPECT_EQ(1u,
            ModuleIdCount(m2->first.module_id, p1->second.unloaded_modules));

  // Start a process.
  module_database_->OnProcessStarted(kPid2, kCreateTime2,
                                     content::PROCESS_TYPE_RENDERER);
  EXPECT_EQ(2u, modules().size());
  EXPECT_EQ(2u, processes().size());
  auto p2 = processes().rbegin();
  EXPECT_EQ(kPid2, p2->first.process_id);
  EXPECT_EQ(kCreateTime2, p2->first.creation_time);
  EXPECT_EQ(content::PROCESS_TYPE_RENDERER, p2->first.process_type);
  EXPECT_EQ(0u, p2->second.loaded_modules.size());
  EXPECT_EQ(0u, p2->second.unloaded_modules.size());

  // Load the dummy.dll in the second process as well.
  module_database_->OnModuleLoad(kPid2, kCreateTime2, dll1_, kSize1, kTime1,
                                 kGoodAddress1);
  EXPECT_EQ(ProcessTypeToBit(content::PROCESS_TYPE_BROWSER) |
                ProcessTypeToBit(content::PROCESS_TYPE_RENDERER),
            m1->second.process_types);
  EXPECT_EQ(kPid2, p2->first.process_id);
  EXPECT_EQ(kCreateTime2, p2->first.creation_time);
  EXPECT_EQ(content::PROCESS_TYPE_RENDERER, p2->first.process_type);
  EXPECT_EQ(1u, p2->second.loaded_modules.size());
  EXPECT_EQ(0u, p2->second.unloaded_modules.size());
  EXPECT_EQ(1u, ModuleIdCount(m1->first.module_id, p2->second.loaded_modules));

  // End the second process without an explicit unload. This invalidates |p2|.
  module_database_->OnProcessEnded(kPid2, kCreateTime2);
  EXPECT_EQ(2u, modules().size());
  EXPECT_EQ(1u, processes().size());
  EXPECT_EQ(kPid1, p1->first.process_id);
  EXPECT_EQ(kCreateTime1, p1->first.creation_time);
  EXPECT_EQ(ProcessTypeToBit(content::PROCESS_TYPE_BROWSER) |
                ProcessTypeToBit(content::PROCESS_TYPE_RENDERER),
            m1->second.process_types);
  EXPECT_EQ(ProcessTypeToBit(content::PROCESS_TYPE_BROWSER),
            m2->second.process_types);

  // End the first process without an explicit unload. This invalidates |p1|.
  module_database_->OnProcessEnded(kPid1, kCreateTime1);
  EXPECT_EQ(2u, modules().size());
  EXPECT_EQ(0u, processes().size());
  EXPECT_EQ(ProcessTypeToBit(content::PROCESS_TYPE_BROWSER) |
                ProcessTypeToBit(content::PROCESS_TYPE_RENDERER),
            m1->second.process_types);
  EXPECT_EQ(ProcessTypeToBit(content::PROCESS_TYPE_BROWSER),
            m2->second.process_types);
}
