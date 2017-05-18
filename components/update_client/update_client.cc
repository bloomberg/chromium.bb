// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_client.h"

#include <algorithm>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/ping_manager.h"
#include "components/update_client/protocol_parser.h"
#include "components/update_client/task_send_uninstall_ping.h"
#include "components/update_client/task_update.h"
#include "components/update_client/update_checker.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/update_client_internal.h"
#include "components/update_client/update_engine.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

namespace update_client {

CrxUpdateItem::CrxUpdateItem() : state(ComponentState::kNew) {}

CrxUpdateItem::~CrxUpdateItem() {
}

CrxUpdateItem::CrxUpdateItem(const CrxUpdateItem& other) = default;

CrxComponent::CrxComponent()
    : allows_background_download(true),
      requires_network_encryption(true),
      supports_group_policy_enable_component_updates(false) {}

CrxComponent::CrxComponent(const CrxComponent& other) = default;

CrxComponent::~CrxComponent() {
}

// It is important that an instance of the UpdateClient binds an unretained
// pointer to itself. Otherwise, a life time circular dependency between this
// instance and its inner members prevents the destruction of this instance.
// Using unretained references is allowed in this case since the life time of
// the UpdateClient instance exceeds the life time of its inner members,
// including any thread objects that might execute callbacks bound to it.
UpdateClientImpl::UpdateClientImpl(
    const scoped_refptr<Configurator>& config,
    std::unique_ptr<PingManager> ping_manager,
    UpdateChecker::Factory update_checker_factory,
    CrxDownloader::Factory crx_downloader_factory)
    : is_stopped_(false),
      config_(config),
      ping_manager_(std::move(ping_manager)),
      update_engine_(base::MakeUnique<UpdateEngine>(
          config,
          update_checker_factory,
          crx_downloader_factory,
          ping_manager_.get(),
          base::Bind(&UpdateClientImpl::NotifyObservers,
                     base::Unretained(this)))) {}

UpdateClientImpl::~UpdateClientImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(task_queue_.empty());
  DCHECK(tasks_.empty());

  config_ = nullptr;
}

void UpdateClientImpl::Install(const std::string& id,
                               const CrxDataCallback& crx_data_callback,
                               const Callback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (IsUpdating(id)) {
    callback.Run(Error::UPDATE_IN_PROGRESS);
    return;
  }

  std::vector<std::string> ids = {id};

  // Partially applies |callback| to OnTaskComplete, so this argument is
  // available when the task completes, along with the task itself.
  std::unique_ptr<TaskUpdate> task = base::MakeUnique<TaskUpdate>(
      update_engine_.get(), true, ids, crx_data_callback,
      base::Bind(&UpdateClientImpl::OnTaskComplete, this, callback));

  // Install tasks are run concurrently and never queued up.
  RunTask(std::move(task));
}

void UpdateClientImpl::Update(const std::vector<std::string>& ids,
                              const CrxDataCallback& crx_data_callback,
                              const Callback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<TaskUpdate> task = base::MakeUnique<TaskUpdate>(
      update_engine_.get(), false, ids, crx_data_callback,
      base::Bind(&UpdateClientImpl::OnTaskComplete, this, callback));

  // If no other tasks are running at the moment, run this update task.
  // Otherwise, queue the task up.
  if (tasks_.empty()) {
    RunTask(std::move(task));
  } else {
    task_queue_.push(task.release());
  }
}

void UpdateClientImpl::RunTask(std::unique_ptr<Task> task) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&Task::Run, base::Unretained(task.get())));
  tasks_.insert(task.release());
}

void UpdateClientImpl::OnTaskComplete(const Callback& callback,
                                      Task* task,
                                      Error error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(task);

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, error));

  // Remove the task from the set of the running tasks. Only tasks handled by
  // the update engine can be in this data structure.
  tasks_.erase(task);

  // Delete the completed task. A task can be completed because the update
  // engine has run it or because it has been canceled but never run.
  delete task;

  if (is_stopped_)
    return;

  // Pick up a task from the queue if the queue has pending tasks and no other
  // task is running.
  if (tasks_.empty() && !task_queue_.empty()) {
    RunTask(std::unique_ptr<Task>(task_queue_.front()));
    task_queue_.pop();
  }
}

void UpdateClientImpl::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void UpdateClientImpl::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

void UpdateClientImpl::NotifyObservers(Observer::Events event,
                                       const std::string& id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : observer_list_)
    observer.OnEvent(event, id);
}

bool UpdateClientImpl::GetCrxUpdateState(const std::string& id,
                                         CrxUpdateItem* update_item) const {
  return update_engine_->GetUpdateState(id, update_item);
}

bool UpdateClientImpl::IsUpdating(const std::string& id) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (const auto* task : tasks_) {
    const auto ids(task->GetIds());
    if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
      return true;
    }
  }

  return false;
}

void UpdateClientImpl::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());

  is_stopped_ = true;

  // In the current implementation it is sufficient to cancel the pending
  // tasks only. The tasks that are run by the update engine will stop
  // making progress naturally, as the main task runner stops running task
  // actions. Upon the browser shutdown, the resources employed by the active
  // tasks will leak, as the operating system kills the thread associated with
  // the update engine task runner. Further refactoring may be needed in this
  // area, to cancel the running tasks by canceling the current action update.
  // This behavior would be expected, correct, and result in no resource leaks
  // in all cases, in shutdown or not.
  //
  // Cancel the pending tasks. These tasks are safe to cancel and delete since
  // they have not picked up by the update engine, and not shared with any
  // task runner yet.
  while (!task_queue_.empty()) {
    auto* task(task_queue_.front());
    task_queue_.pop();
    task->Cancel();
  }
}

void UpdateClientImpl::SendUninstallPing(const std::string& id,
                                         const base::Version& version,
                                         int reason,
                                         const Callback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<TaskSendUninstallPing> task =
      base::MakeUnique<TaskSendUninstallPing>(
          update_engine_.get(), id, version, reason,
          base::Bind(&UpdateClientImpl::OnTaskComplete, base::Unretained(this),
                     callback));
  RunTask(std::move(task));
}

scoped_refptr<UpdateClient> UpdateClientFactory(
    const scoped_refptr<Configurator>& config) {
  return base::MakeRefCounted<UpdateClientImpl>(
      config, base::MakeUnique<PingManager>(config), &UpdateChecker::Create,
      &CrxDownloader::Create);
}

void RegisterPrefs(PrefRegistrySimple* registry) {
  PersistedData::RegisterPrefs(registry);
}

}  // namespace update_client
