// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_pool.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool/thread_group_params.h"
#include "base/task/thread_pool/thread_pool_impl.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"

namespace base {

namespace {

// |g_thread_pool| is intentionally leaked on shutdown.
ThreadPool* g_thread_pool = nullptr;

}  // namespace

ThreadPool::InitParams::InitParams(
    const ThreadGroupParams& background_thread_group_params_in,
    const ThreadGroupParams& foreground_thread_group_params_in,
    CommonThreadPoolEnvironment common_thread_pool_environment_in)
    : background_thread_group_params(background_thread_group_params_in),
      foreground_thread_group_params(foreground_thread_group_params_in),
      common_thread_pool_environment(common_thread_pool_environment_in) {}

ThreadPool::InitParams::~InitParams() = default;

ThreadPool::ScopedExecutionFence::ScopedExecutionFence() {
  DCHECK(g_thread_pool);
  g_thread_pool->SetCanRun(false);
}

ThreadPool::ScopedExecutionFence::~ScopedExecutionFence() {
  DCHECK(g_thread_pool);
  g_thread_pool->SetCanRun(true);
}

#if !defined(OS_NACL)
// static
void ThreadPool::CreateAndStartWithDefaultParams(StringPiece name) {
  Create(name);
  GetInstance()->StartWithDefaultParams();
}

void ThreadPool::StartWithDefaultParams() {
  // Values were chosen so that:
  // * There are few background threads.
  // * Background threads never outnumber foreground threads.
  // * The system is utilized maximally by foreground threads.
  // * The main thread is assumed to be busy, cap foreground workers at
  //   |num_cores - 1|.
  const int num_cores = SysInfo::NumberOfProcessors();

  // TODO(etiennep): Change this to 2.
  constexpr int kBackgroundMaxThreads = 3;
  const int kForegroundMaxThreads = std::max(3, num_cores - 1);

  constexpr TimeDelta kSuggestedReclaimTime = TimeDelta::FromSeconds(30);

  Start({{kBackgroundMaxThreads, kSuggestedReclaimTime},
         {kForegroundMaxThreads, kSuggestedReclaimTime}});
}
#endif  // !defined(OS_NACL)

void ThreadPool::Create(StringPiece name) {
  SetInstance(std::make_unique<internal::ThreadPoolImpl>(name));
}

// static
void ThreadPool::SetInstance(std::unique_ptr<ThreadPool> thread_pool) {
  delete g_thread_pool;
  g_thread_pool = thread_pool.release();
}

// static
ThreadPool* ThreadPool::GetInstance() {
  return g_thread_pool;
}

}  // namespace base
