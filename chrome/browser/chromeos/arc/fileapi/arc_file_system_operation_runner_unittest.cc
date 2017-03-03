// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/file_system.mojom.h"
#include "components/arc/test/fake_file_system_instance.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace arc {

namespace {

constexpr char kAuthority[] = "authority";
constexpr char kDocumentId[] = "document_id";
constexpr char kUrl[] = "content://test";

}  // namespace

class ArcFileSystemOperationRunnerTest : public testing::Test {
 public:
  ArcFileSystemOperationRunnerTest() = default;
  ~ArcFileSystemOperationRunnerTest() override = default;

  void SetUp() override {
    arc_service_manager_ = base::MakeUnique<ArcServiceManager>(nullptr);
    arc_service_manager_->arc_bridge_service()->file_system()->SetInstance(
        &file_system_instance_);
    arc_service_manager_->AddService(
        ArcFileSystemOperationRunner::CreateForTesting(
            arc_service_manager_->arc_bridge_service()));

    // Run the message loop until FileSystemInstance::Init() is called.
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(file_system_instance_.InitCalled());

    runner_ = arc_service_manager_->GetService<ArcFileSystemOperationRunner>();
  }

 protected:
  // Calls private ArcFileSystemOperationRunner::SetShouldDefer().
  void CallSetShouldDefer(bool should_defer) {
    runner_->SetShouldDefer(should_defer);
  }

  // Calls all functions implemented by ArcFileSystemOperationRunner.
  void CallAllFunctions(int* counter) {
    // Following functions are deferred.
    runner_->AddWatcher(
        kAuthority, kDocumentId,
        base::Bind([](ArcFileSystemOperationRunner::ChangeType type) {}),
        base::Bind([](int* counter, int64_t watcher_id) { ++*counter; },
                   counter));
    runner_->GetChildDocuments(
        kAuthority, kDocumentId,
        base::Bind(
            [](int* counter,
               base::Optional<std::vector<mojom::DocumentPtr>> documents) {
              ++*counter;
            },
            counter));
    runner_->GetDocument(
        kAuthority, kDocumentId,
        base::Bind(
            [](int* counter, mojom::DocumentPtr document) { ++*counter; },
            counter));
    runner_->GetFileSize(
        GURL(kUrl),
        base::Bind([](int* counter, int64_t size) { ++*counter; }, counter));
    runner_->OpenFileToRead(
        GURL(kUrl),
        base::Bind([](int* counter, mojo::ScopedHandle handle) { ++*counter; },
                   counter));

    // RemoveWatcher() is never deferred.
    runner_->RemoveWatcher(
        123,
        base::Bind([](int* counter, bool success) { ++*counter; }, counter));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  FakeFileSystemInstance file_system_instance_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  // Owned by |arc_service_manager_|.
  ArcFileSystemOperationRunner* runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcFileSystemOperationRunnerTest);
};

TEST_F(ArcFileSystemOperationRunnerTest, RunImmediately) {
  int counter = 0;
  CallSetShouldDefer(false);
  CallAllFunctions(&counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(6, counter);
}

TEST_F(ArcFileSystemOperationRunnerTest, DeferAndRun) {
  int counter = 0;
  CallSetShouldDefer(true);
  CallAllFunctions(&counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, counter);

  CallSetShouldDefer(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(6, counter);
}

TEST_F(ArcFileSystemOperationRunnerTest, DeferAndDiscard) {
  int counter = 0;
  CallSetShouldDefer(true);
  CallAllFunctions(&counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, counter);

  arc_service_manager_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, counter);
}

TEST_F(ArcFileSystemOperationRunnerTest, FileInstanceUnavailable) {
  arc_service_manager_->arc_bridge_service()->file_system()->SetInstance(
      nullptr);

  int counter = 0;
  CallSetShouldDefer(false);
  CallAllFunctions(&counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(6, counter);
}

}  // namespace arc
