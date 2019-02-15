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

#if defined(OS_ANDROID)
#include "base/android/task_scheduler/post_task_android.h"
#endif

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
    std::unique_ptr<base::MessageLoopForUI> message_loop,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner)
    : message_loop_(std::move(message_loop)),
      ui_thread_task_runner_(std::move(ui_thread_task_runner)) {}

BrowserTaskExecutor::~BrowserTaskExecutor() = default;

// static
void BrowserTaskExecutor::Create() {
  DCHECK(!g_browser_task_executor);
  DCHECK(!base::ThreadTaskRunnerHandle::IsSet());
  std::unique_ptr<base::MessageLoopForUI> message_loop =
      std::make_unique<base::MessageLoopForUI>();
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner =
      message_loop->task_runner();
  g_browser_task_executor = new BrowserTaskExecutor(
      std::move(message_loop), std::move(ui_thread_task_runner));
  base::RegisterTaskExecutor(BrowserTaskTraitsExtension::kExtensionId,
                             g_browser_task_executor);
#if defined(OS_ANDROID)
  base::PostTaskAndroid::SignalNativeSchedulerReady();
#endif
}

// static
void BrowserTaskExecutor::CreateForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner) {
  DCHECK(!g_browser_task_executor);
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());
  DCHECK(ui_thread_task_runner);
  g_browser_task_executor =
      new BrowserTaskExecutor(nullptr, std::move(ui_thread_task_runner));
  base::RegisterTaskExecutor(BrowserTaskTraitsExtension::kExtensionId,
                             g_browser_task_executor);
#if defined(OS_ANDROID)
  base::PostTaskAndroid::SignalNativeSchedulerReady();
#endif
}

// static
void BrowserTaskExecutor::PostFeatureListSetup() {
  DCHECK(g_browser_task_executor);
  // TODO(alexclarke): Forward to the BrowserUIThreadScheduler.
}

// static
void BrowserTaskExecutor::Shutdown() {
  if (!g_browser_task_executor)
    return;

  DCHECK(g_browser_task_executor->message_loop_);
  // We don't delete either |g_browser_task_executor| or because other threads
  // may PostTask or call BrowserTaskExecutor::GetTaskRunner while  we're
  // tearing things down. We don't want to add locks so we just leak instead of
  // dealing with that.For similar reasons we don't need to call
  // PostTaskAndroid::SignalNativeSchedulerShutdown on Android. In tests however
  // we need to clean up, so BrowserTaskExecutor::ResetForTesting should be
  // called.
  g_browser_task_executor->message_loop_.reset();
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
  DCHECK_LT(thread_id, BrowserThread::ID::ID_COUNT);
  // TODO(eseckler): For now, make BEST_EFFORT tasks run after startup. Once the
  // BrowserUIThreadScheduler is in place, this should be handled by its
  // policies instead.
  if (traits.priority() == base::TaskPriority::BEST_EFFORT)
    return GetAfterStartupTaskRunnerForThread(thread_id);
  if (thread_id == BrowserThread::UI)
    return ui_thread_task_runner_;
  return GetProxyTaskRunnerForThread(thread_id);
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
