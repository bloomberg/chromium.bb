// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_environment.h"

#include "ash/test/ash_test_views_delegate.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/threading/sequenced_worker_pool.h"

namespace ash {
namespace test {
namespace {

class AshTestEnvironmentDefault : public AshTestEnvironment {
 public:
  AshTestEnvironmentDefault() {}

  ~AshTestEnvironmentDefault() override {
    base::RunLoop().RunUntilIdle();
    blocking_pool_owner_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // AshTestEnvironment:
  base::SequencedWorkerPool* GetBlockingPool() override {
    if (!blocking_pool_owner_) {
      const size_t kMaxNumberThreads = 3u;  // Matches that of content.
      const char kThreadNamePrefix[] = "AshBlocking";
      blocking_pool_owner_ = base::MakeUnique<base::SequencedWorkerPoolOwner>(
          kMaxNumberThreads, kThreadNamePrefix);
    }
    return blocking_pool_owner_->pool().get();
  }
  std::unique_ptr<AshTestViewsDelegate> CreateViewsDelegate() override {
    return base::MakeUnique<AshTestViewsDelegate>();
  }

 private:
  base::MessageLoopForUI message_loop_;
  std::unique_ptr<base::SequencedWorkerPoolOwner> blocking_pool_owner_;

  DISALLOW_COPY_AND_ASSIGN(AshTestEnvironmentDefault);
};

}  // namespace

// static
std::unique_ptr<AshTestEnvironment> AshTestEnvironment::Create() {
  return base::MakeUnique<AshTestEnvironmentDefault>();
}

// static
std::string AshTestEnvironment::Get100PercentResourceFileName() {
  return "ash_test_resources_100_percent.pak";
}

}  // namespace test
}  // namespace ash
