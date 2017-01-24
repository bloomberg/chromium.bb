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
#include "chrome/browser/chromeos/arc/fileapi/arc_deferred_file_system_operation_runner.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/file_system.mojom.h"
#include "components/arc/file_system/arc_file_system_operation_runner.h"
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

class ArcDeferredFileSystemOperationRunnerTest : public testing::Test {
 public:
  ArcDeferredFileSystemOperationRunnerTest() = default;
  ~ArcDeferredFileSystemOperationRunnerTest() override = default;

  void SetUp() override {
    arc_service_manager_ = base::MakeUnique<ArcServiceManager>(nullptr);
    arc_service_manager_->arc_bridge_service()->file_system()->SetInstance(
        &file_system_instance_);
    // We can not use base::MakeUnique() for friend constructors.
    arc_service_manager_->AddService(
        base::WrapUnique(new ArcDeferredFileSystemOperationRunner(
            arc_service_manager_->arc_bridge_service(),
            false /* observe_events */)));
    deferred_runner_ = static_cast<ArcDeferredFileSystemOperationRunner*>(
        arc_service_manager_->GetService<ArcFileSystemOperationRunner>());
  }

 protected:
  // Calls private ArcDeferredFileSystemOperationRunner::SetShouldDefer().
  void CallSetShouldDefer(bool should_defer) {
    deferred_runner_->SetShouldDefer(should_defer);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  FakeFileSystemInstance file_system_instance_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  // Owned by |arc_service_manager_|.
  ArcDeferredFileSystemOperationRunner* deferred_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcDeferredFileSystemOperationRunnerTest);
};

TEST_F(ArcDeferredFileSystemOperationRunnerTest, RunImmediately) {
  int counter = 0;
  CallSetShouldDefer(false);
  deferred_runner_->GetChildDocuments(
      kAuthority, kDocumentId,
      base::Bind(
          [](int* counter,
             base::Optional<std::vector<mojom::DocumentPtr>> documents) {
            ++*counter;
          },
          &counter));
  deferred_runner_->GetDocument(
      kAuthority, kDocumentId,
      base::Bind([](int* counter, mojom::DocumentPtr document) { ++*counter; },
                 &counter));
  deferred_runner_->GetFileSize(
      GURL(kUrl),
      base::Bind([](int* counter, int64_t size) { ++*counter; }, &counter));
  deferred_runner_->OpenFileToRead(
      GURL(kUrl),
      base::Bind([](int* counter, mojo::ScopedHandle handle) { ++*counter; },
                 &counter));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, counter);
}

TEST_F(ArcDeferredFileSystemOperationRunnerTest, DeferAndRun) {
  int counter = 0;
  CallSetShouldDefer(true);
  deferred_runner_->GetChildDocuments(
      kAuthority, kDocumentId,
      base::Bind(
          [](int* counter,
             base::Optional<std::vector<mojom::DocumentPtr>> documents) {
            ++*counter;
          },
          &counter));
  deferred_runner_->GetDocument(
      kAuthority, kDocumentId,
      base::Bind([](int* counter, mojom::DocumentPtr document) { ++*counter; },
                 &counter));
  deferred_runner_->GetFileSize(
      GURL(kUrl),
      base::Bind([](int* counter, int64_t size) { ++*counter; }, &counter));
  deferred_runner_->OpenFileToRead(
      GURL(kUrl),
      base::Bind([](int* counter, mojo::ScopedHandle handle) { ++*counter; },
                 &counter));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, counter);

  CallSetShouldDefer(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, counter);
}

TEST_F(ArcDeferredFileSystemOperationRunnerTest, DeferAndDiscard) {
  int counter = 0;
  CallSetShouldDefer(true);
  deferred_runner_->GetChildDocuments(
      kAuthority, kDocumentId,
      base::Bind(
          [](int* counter,
             base::Optional<std::vector<mojom::DocumentPtr>> documents) {
            ++*counter;
          },
          &counter));
  deferred_runner_->GetDocument(
      kAuthority, kDocumentId,
      base::Bind([](int* counter, mojom::DocumentPtr document) { ++*counter; },
                 &counter));
  deferred_runner_->GetFileSize(
      GURL(kUrl),
      base::Bind([](int* counter, int64_t size) { ++*counter; }, &counter));
  deferred_runner_->OpenFileToRead(
      GURL(kUrl),
      base::Bind([](int* counter, mojo::ScopedHandle handle) { ++*counter; },
                 &counter));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, counter);

  arc_service_manager_.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, counter);
}

TEST_F(ArcDeferredFileSystemOperationRunnerTest, FileInstanceUnavailable) {
  arc_service_manager_->arc_bridge_service()->file_system()->SetInstance(
      nullptr);

  int counter = 0;
  CallSetShouldDefer(false);
  deferred_runner_->GetChildDocuments(
      kAuthority, kDocumentId,
      base::Bind(
          [](int* counter,
             base::Optional<std::vector<mojom::DocumentPtr>> documents) {
            ++*counter;
          },
          &counter));
  deferred_runner_->GetDocument(
      kAuthority, kDocumentId,
      base::Bind([](int* counter, mojom::DocumentPtr document) { ++*counter; },
                 &counter));
  deferred_runner_->GetFileSize(
      GURL(kUrl),
      base::Bind([](int* counter, int64_t size) { ++*counter; }, &counter));
  deferred_runner_->OpenFileToRead(
      GURL(kUrl),
      base::Bind([](int* counter, mojo::ScopedHandle handle) { ++*counter; },
                 &counter));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, counter);
}

}  // namespace arc
