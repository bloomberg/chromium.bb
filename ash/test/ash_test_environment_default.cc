// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_environment.h"

#include "ash/test/ash_test_views_delegate.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_worker_pool.h"

namespace ash {
namespace test {
namespace {

class AshTestEnvironmentDefault : public AshTestEnvironment {
 public:
  AshTestEnvironmentDefault() {}

  ~AshTestEnvironmentDefault() override {
    base::RunLoop().RunUntilIdle();
    if (blocking_pool_) {
      blocking_pool_->FlushForTesting();
      blocking_pool_->Shutdown();
      blocking_pool_ = nullptr;
    }
    base::RunLoop().RunUntilIdle();
  }

  // AshTestEnvironment:
  base::SequencedWorkerPool* GetBlockingPool() override {
    if (!blocking_pool_) {
      const size_t kMaxNumberThreads = 3u;  // Matches that of content.
      blocking_pool_ = new base::SequencedWorkerPool(
          kMaxNumberThreads, "AshBlocking", base::TaskPriority::USER_VISIBLE);
    }
    return blocking_pool_.get();
  }
  std::unique_ptr<views::ViewsDelegate> CreateViewsDelegate() override {
    return base::MakeUnique<AshTestViewsDelegate>();
  }

 private:
  base::MessageLoopForUI message_loop_;
  scoped_refptr<base::SequencedWorkerPool> blocking_pool_;

  DISALLOW_COPY_AND_ASSIGN(AshTestEnvironmentDefault);
};

}  // namespace

// static
std::unique_ptr<AshTestEnvironment> AshTestEnvironment::Create() {
  return base::MakeUnique<AshTestEnvironmentDefault>();
}

}  // namespace test
}  // namespace ash
