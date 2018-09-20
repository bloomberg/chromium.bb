// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_task_executor.h"

#include "content/browser/browser_thread_impl.h"

namespace content {
namespace {

// |g_browser_task_executor| is intentionally leaked on shutdown.
BrowserTaskExecutor* g_browser_task_executor = nullptr;

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
  DCHECK_LT(thread_id, BrowserThread::ID::ID_COUNT);
  return BrowserThreadImpl::GetTaskRunnerForThread(thread_id);
}

}  // namespace content
