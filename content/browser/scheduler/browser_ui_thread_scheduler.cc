// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_ui_thread_scheduler.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/time_domain.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"

namespace content {

BrowserUIThreadScheduler::~BrowserUIThreadScheduler() = default;

// static
std::unique_ptr<BrowserUIThreadScheduler>
BrowserUIThreadScheduler::CreateForTesting(
    base::sequence_manager::SequenceManager* sequence_manager,
    base::sequence_manager::TimeDomain* time_domain) {
  return base::WrapUnique(
      new BrowserUIThreadScheduler(sequence_manager, time_domain));
}

void BrowserUIThreadScheduler::PostFeatureListSetup() {
  // TODO(scheduler-dev): Initialize experiments here.
}

void BrowserUIThreadScheduler::Shutdown() {
  task_queues_.clear();
  owned_sequence_manager_.reset();
  sequence_manager_ = nullptr;
}

BrowserUIThreadScheduler::BrowserUIThreadScheduler()
    : owned_sequence_manager_(
          base::sequence_manager::CreateUnboundSequenceManager(
              base::sequence_manager::SequenceManager::Settings{
                  .message_loop_type = base::MessageLoop::TYPE_UI})),
      sequence_manager_(owned_sequence_manager_.get()),
      time_domain_(sequence_manager_->GetRealTimeDomain()) {
  InitialiseTaskQueues();

  sequence_manager_->SetDefaultTaskRunner(GetTaskRunner(QueueType::kDefault));

  sequence_manager_->BindToMessagePump(
      base::MessageLoop::CreateMessagePumpForType(base::MessageLoop::TYPE_UI));
}

BrowserUIThreadScheduler::BrowserUIThreadScheduler(
    base::sequence_manager::SequenceManager* sequence_manager,
    base::sequence_manager::TimeDomain* time_domain)
    : sequence_manager_(sequence_manager), time_domain_(time_domain) {
  InitialiseTaskQueues();
}

void BrowserUIThreadScheduler::InitialiseTaskQueues() {
  DCHECK(sequence_manager_);
  sequence_manager_->EnableCrashKeys("ui_scheduler_task_file_name",
                                     "ui_scheduler_task_function_name",
                                     "ui_scheduler_async_stack");

  // To avoid locks in BrowserUIThreadScheduler::GetTaskRunner, eagerly
  // create all the well known task queues.
  for (int i = 0;
       i < static_cast<int>(BrowserUIThreadTaskQueue::QueueType::kCount); i++) {
    BrowserUIThreadTaskQueue::QueueType queue_type =
        static_cast<BrowserUIThreadTaskQueue::QueueType>(i);
    scoped_refptr<BrowserUIThreadTaskQueue> task_queue =
        sequence_manager_->CreateTaskQueueWithType<BrowserUIThreadTaskQueue>(
            base::sequence_manager::TaskQueue::Spec(
                BrowserUIThreadTaskQueue::NameForQueueType(queue_type))
                .SetTimeDomain(time_domain_),
            queue_type);
    task_queues_.emplace(queue_type, task_queue);
    task_runners_.emplace(queue_type, task_queue->task_runner());
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
BrowserUIThreadScheduler::GetTaskRunnerForTesting(QueueType queue_type) {
  return GetTaskRunner(queue_type);
}

scoped_refptr<base::SingleThreadTaskRunner>
BrowserUIThreadScheduler::GetTaskRunner(QueueType queue_type) {
  auto it = task_runners_.find(queue_type);
  if (it != task_runners_.end())
    return it->second;
  NOTREACHED();
  return scoped_refptr<base::SingleThreadTaskRunner>();
}

}  // namespace content
