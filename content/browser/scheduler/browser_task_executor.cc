// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_task_executor.h"

#include <atomic>

#include "base/bind.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/no_destructor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/scheduler/browser_ui_thread_scheduler.h"
#include "content/public/browser/browser_task_traits.h"

#if defined(OS_ANDROID)
#include "base/android/task_scheduler/post_task_android.h"
#endif

namespace content {
namespace {

using QueueType = content::BrowserUIThreadTaskQueue::QueueType;

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

// TODO(eseckler): This should be replaced by the BrowserUIThreadScheduler.
class AfterStartupTaskRunner : public base::SingleThreadTaskRunner {
 public:
  explicit AfterStartupTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> proxied_task_runner)
      : proxied_task_runner_(proxied_task_runner) {
    Reset();
  }

  void Reset() {
    listening_for_startup_ = false;
    deferred_task_runner_ =
        base::MakeRefCounted<base::DeferredSequencedTaskRunner>(
            proxied_task_runner_);
  }

  // SingleThreadTaskRunner implementation.
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    EnsureListeningForStartup();
    return deferred_task_runner_->PostDelayedTask(from_here, std::move(task),
                                                  delay);
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    EnsureListeningForStartup();
    return deferred_task_runner_->PostNonNestableDelayedTask(
        from_here, std::move(task), delay);
  }

  bool RunsTasksInCurrentSequence() const override {
    return deferred_task_runner_->RunsTasksInCurrentSequence();
  }

  void EnsureListeningForStartup() {
    if (!listening_for_startup_.exchange(true)) {
      BrowserThread::PostAfterStartupTask(
          FROM_HERE, proxied_task_runner_,
          base::BindOnce(&AfterStartupTaskRunner::Start,
                         base::Unretained(this)));
    }
  }

 private:
  ~AfterStartupTaskRunner() override {}

  void Start() { deferred_task_runner_->Start(); }

  std::atomic_bool listening_for_startup_;
  scoped_refptr<SingleThreadTaskRunner> proxied_task_runner_;
  scoped_refptr<base::DeferredSequencedTaskRunner> deferred_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AfterStartupTaskRunner);
};

scoped_refptr<BrowserThreadTaskRunner> GetProxyTaskRunnerForThreadImpl(
    BrowserThread::ID id) {
  using TaskRunnerMap = std::array<scoped_refptr<BrowserThreadTaskRunner>,
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

scoped_refptr<AfterStartupTaskRunner> GetAfterStartupTaskRunnerForThreadImpl(
    BrowserThread::ID id) {
  using TaskRunnerMap = std::array<scoped_refptr<AfterStartupTaskRunner>,
                                   BrowserThread::ID_COUNT>;
  static const base::NoDestructor<TaskRunnerMap> task_runners([] {
    TaskRunnerMap task_runners;
    for (int i = 0; i < BrowserThread::ID_COUNT; ++i)
      task_runners[i] = base::MakeRefCounted<AfterStartupTaskRunner>(
          GetProxyTaskRunnerForThreadImpl(static_cast<BrowserThread::ID>(i)));
    return task_runners;
  }());
  return (*task_runners)[id];
}

}  // namespace

BrowserTaskExecutor::BrowserTaskExecutor(
    std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler)
    : browser_ui_thread_scheduler_(std::move(browser_ui_thread_scheduler)) {}

BrowserTaskExecutor::~BrowserTaskExecutor() = default;

// static
void BrowserTaskExecutor::Create() {
  DCHECK(!g_browser_task_executor);
  DCHECK(!base::ThreadTaskRunnerHandle::IsSet());
  g_browser_task_executor =
      new BrowserTaskExecutor(std::make_unique<BrowserUIThreadScheduler>());
  base::RegisterTaskExecutor(BrowserTaskTraitsExtension::kExtensionId,
                             g_browser_task_executor);
#if defined(OS_ANDROID)
  base::PostTaskAndroid::SignalNativeSchedulerReady();
#endif
}

// static
void BrowserTaskExecutor::CreateWithBrowserUIThreadSchedulerForTesting(
    std::unique_ptr<BrowserUIThreadScheduler> browser_ui_thread_scheduler) {
  DCHECK(!g_browser_task_executor);
  g_browser_task_executor =
      new BrowserTaskExecutor(std::move(browser_ui_thread_scheduler));
  base::RegisterTaskExecutor(BrowserTaskTraitsExtension::kExtensionId,
                             g_browser_task_executor);
#if defined(OS_ANDROID)
  base::PostTaskAndroid::SignalNativeSchedulerReady();
#endif
}

// static
void BrowserTaskExecutor::PostFeatureListSetup() {
  DCHECK(g_browser_task_executor);
  g_browser_task_executor->browser_ui_thread_scheduler_->PostFeatureListSetup();
}

// static
void BrowserTaskExecutor::Shutdown() {
  if (!g_browser_task_executor)
    return;

  DCHECK(g_browser_task_executor->browser_ui_thread_scheduler_);
  // We don't delete either |g_browser_task_executor| or the
  // BrowserUIThreadScheduler it owns because other threads may PostTask or call
  // BrowserTaskExecutor::GetTaskRunner while we're tearing things down. We
  // don't want to add locks so we just leak instead of dealing with that.
  // For similar reasons we don't need to call
  // PostTaskAndroid::SignalNativeSchedulerShutdown on Android. In tests however
  // we need to clean up, so BrowserTaskExecutor::ResetForTesting should be
  // called.
  g_browser_task_executor->browser_ui_thread_scheduler_->Shutdown();
}

// static
void BrowserTaskExecutor::ResetForTesting() {
#if defined(OS_ANDROID)
  base::PostTaskAndroid::SignalNativeSchedulerShutdown();
#endif

  for (int i = 0; i < BrowserThread::ID_COUNT; ++i) {
    GetAfterStartupTaskRunnerForThreadImpl(static_cast<BrowserThread::ID>(i))
        ->Reset();
  }
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
    return GetTaskRunner(traits, extension)
        ->PostDelayedTask(from_here, std::move(task), delay);
  } else {
    return GetTaskRunner(traits, extension)
        ->PostNonNestableDelayedTask(from_here, std::move(task), delay);
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
  return GetTaskRunner(traits, extension);
}

scoped_refptr<base::SingleThreadTaskRunner> BrowserTaskExecutor::GetTaskRunner(
    const base::TaskTraits& traits,
    const BrowserTaskTraitsExtension& extension) {
  BrowserThread::ID thread_id = extension.browser_thread();
  DCHECK_GE(thread_id, 0);
  DCHECK_LT(thread_id, BrowserThread::ID_COUNT);
  if (thread_id != BrowserThread::UI) {
    // TODO(scheduler-dev): Consider adding a simple (static priority based)
    // scheduler to the IO thread and getting rid of AfterStartupTaskRunner.
    if (traits.priority() == base::TaskPriority::BEST_EFFORT)
      return GetAfterStartupTaskRunnerForThread(thread_id);
    return GetProxyTaskRunnerForThread(thread_id);
  }

  BrowserTaskType task_type = extension.task_type();
  DCHECK_LT(task_type, BrowserTaskType::kBrowserTaskType_Last);
  switch (task_type) {
    case BrowserTaskType::kBootstrap:
      // TODO(alexclarke): Lets do this at compile time instead.
      DCHECK(!traits.priority_set_explicitly())
          << "Combining BrowserTaskType and TaskPriority is not currently"
             " supported.";
      return browser_ui_thread_scheduler_->GetTaskRunner(QueueType::kBootstrap);

    case BrowserTaskType::kNavigation:
      // TODO(alexclarke): Lets do this at compile time instead.
      DCHECK(!traits.priority_set_explicitly())
          << "Combining BrowserTaskType and TaskPriority is not currently"
             " supported.";
      return browser_ui_thread_scheduler_->GetTaskRunner(
          QueueType::kNavigation);

    case BrowserTaskType::kDefault:
      // Defer to traits.priority() below.
      break;

    case BrowserTaskType::kBrowserTaskType_Last:
      NOTREACHED();
  }

  switch (traits.priority()) {
    case base::TaskPriority::BEST_EFFORT:
      // TODO(eseckler): For now, make BEST_EFFORT tasks run after startup. Once
      // the BrowserUIThreadScheduler is in place, this should be handled by its
      // policies instead.
      return GetAfterStartupTaskRunnerForThread(thread_id);

    case base::TaskPriority::USER_VISIBLE:
      return browser_ui_thread_scheduler_->GetTaskRunner(QueueType::kDefault);

    case base::TaskPriority::USER_BLOCKING:
      return browser_ui_thread_scheduler_->GetTaskRunner(
          QueueType::kUserBlocking);
  }
}

// static
scoped_refptr<base::SingleThreadTaskRunner>
BrowserTaskExecutor::GetProxyTaskRunnerForThread(BrowserThread::ID id) {
  return GetProxyTaskRunnerForThreadImpl(id);
}

// static
scoped_refptr<base::SingleThreadTaskRunner>
BrowserTaskExecutor::GetAfterStartupTaskRunnerForThread(BrowserThread::ID id) {
  return GetAfterStartupTaskRunnerForThreadImpl(id);
}

}  // namespace content
