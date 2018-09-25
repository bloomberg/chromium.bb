// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_task_executor.h"

#include "base/no_destructor.h"
#include "content/browser/browser_thread_impl.h"

namespace content {
namespace {

// |g_browser_task_executor| is intentionally leaked on shutdown.
BrowserTaskExecutor* g_browser_task_executor = nullptr;

// An implementation of SingleThreadTaskRunner to be used in conjunction with
// BrowserThread. BrowserThreadTaskRunners are vended by
// base::Create*TaskRunnerWithTraits({BrowserThread::UI/IO}).
//
// TODO(gab): Consider replacing this with direct calls to task runners obtained
// via |BrowserThreadImpl::GetTaskRunnerForThread()| -- only works if none are
// requested before starting the threads.
class BrowserThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  explicit BrowserThreadTaskRunner(BrowserThread::ID identifier)
      : id_(identifier) {}

  // SingleThreadTaskRunner implementation.
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return BrowserThreadImpl::GetTaskRunnerForThread(id_)->PostDelayedTask(
        from_here, std::move(task), delay);
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return BrowserThreadImpl::GetTaskRunnerForThread(id_)
        ->PostNonNestableDelayedTask(from_here, std::move(task), delay);
  }

  bool RunsTasksInCurrentSequence() const override {
    return BrowserThread::CurrentlyOn(id_);
  }

 private:
  ~BrowserThreadTaskRunner() override {}

  const BrowserThread::ID id_;

  DISALLOW_COPY_AND_ASSIGN(BrowserThreadTaskRunner);
};

}  // namespace

BrowserTaskExecutor::BrowserTaskExecutor() = default;
BrowserTaskExecutor::~BrowserTaskExecutor() = default;

// static
void BrowserTaskExecutor::Create() {
  DCHECK(!g_browser_task_executor);
  g_browser_task_executor = new BrowserTaskExecutor();
  base::RegisterTaskExecutor(BrowserTaskTraitsExtension::kExtensionId,
                             g_browser_task_executor);
}

// static
void BrowserTaskExecutor::ResetForTesting() {
  if (g_browser_task_executor) {
    base::UnregisterTaskExecutorForTesting(
        BrowserTaskTraitsExtension::kExtensionId);
    delete g_browser_task_executor;
    g_browser_task_executor = nullptr;
  }
}

bool BrowserTaskExecutor::PostDelayedTaskWithTraits(
    const base::Location& from_here,
    const base::TaskTraits& traits,
    base::OnceClosure task,
    base::TimeDelta delay) {
  DCHECK_EQ(BrowserTaskTraitsExtension::kExtensionId, traits.extension_id());
  const BrowserTaskTraitsExtension& extension =
      traits.GetExtension<BrowserTaskTraitsExtension>();
  if (extension.nestable()) {
    return GetTaskRunner(extension)->PostDelayedTask(from_here, std::move(task),
                                                     delay);
  } else {
    return GetTaskRunner(extension)->PostNonNestableDelayedTask(
        from_here, std::move(task), delay);
  }
}

scoped_refptr<base::TaskRunner> BrowserTaskExecutor::CreateTaskRunnerWithTraits(
    const base::TaskTraits& traits) {
  return GetTaskRunner(traits);
}

scoped_refptr<base::SequencedTaskRunner>
BrowserTaskExecutor::CreateSequencedTaskRunnerWithTraits(
    const base::TaskTraits& traits) {
  return GetTaskRunner(traits);
}

scoped_refptr<base::SingleThreadTaskRunner>
BrowserTaskExecutor::CreateSingleThreadTaskRunnerWithTraits(
    const base::TaskTraits& traits,
    base::SingleThreadTaskRunnerThreadMode thread_mode) {
  return GetTaskRunner(traits);
}

#if defined(OS_WIN)
scoped_refptr<base::SingleThreadTaskRunner>
BrowserTaskExecutor::CreateCOMSTATaskRunnerWithTraits(
    const base::TaskTraits& traits,
    base::SingleThreadTaskRunnerThreadMode thread_mode) {
  return GetTaskRunner(traits);
}
#endif  // defined(OS_WIN)

scoped_refptr<base::SingleThreadTaskRunner> BrowserTaskExecutor::GetTaskRunner(
    const base::TaskTraits& traits) {
  DCHECK_EQ(BrowserTaskTraitsExtension::kExtensionId, traits.extension_id());
  const BrowserTaskTraitsExtension& extension =
      traits.GetExtension<BrowserTaskTraitsExtension>();
  return GetTaskRunner(extension);
}

scoped_refptr<base::SingleThreadTaskRunner> BrowserTaskExecutor::GetTaskRunner(
    const BrowserTaskTraitsExtension& extension) {
  BrowserThread::ID thread_id = extension.browser_thread();
  DCHECK_GE(thread_id, 0);
  DCHECK_LT(thread_id, BrowserThread::ID::ID_COUNT);
  return GetProxyTaskRunnerForThread(thread_id);
}

// static
scoped_refptr<base::SingleThreadTaskRunner>
BrowserTaskExecutor::GetProxyTaskRunnerForThread(BrowserThread::ID id) {
  using TaskRunnerMap = std::array<scoped_refptr<base::SingleThreadTaskRunner>,
                                   BrowserThread::ID_COUNT>;
  static const base::NoDestructor<TaskRunnerMap> task_runners([] {
    TaskRunnerMap task_runners;
    for (int i = 0; i < BrowserThread::ID_COUNT; ++i)
      task_runners[i] = base::MakeRefCounted<BrowserThreadTaskRunner>(
          static_cast<BrowserThread::ID>(i));
    return task_runners;
  }());
  return (*task_runners)[id];
}

}  // namespace content
