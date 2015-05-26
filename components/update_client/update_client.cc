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
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_checker.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/ping_manager.h"
#include "components/update_client/task_update.h"
#include "components/update_client/update_checker.h"
#include "components/update_client/update_client_internal.h"
#include "components/update_client/update_engine.h"
#include "components/update_client/update_response.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

namespace update_client {

CrxUpdateItem::CrxUpdateItem()
    : state(State::kNew),
      on_demand(false),
      diff_update_failed(false),
      error_category(0),
      error_code(0),
      extra_code1(0),
      diff_error_category(0),
      diff_error_code(0),
      diff_extra_code1(0) {
}

CrxUpdateItem::~CrxUpdateItem() {
}

CrxComponent::CrxComponent() : allow_background_download(true) {
}

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
    scoped_ptr<PingManager> ping_manager,
    UpdateChecker::Factory update_checker_factory,
    CrxDownloader::Factory crx_downloader_factory)
    : config_(config),
      ping_manager_(ping_manager.Pass()),
      update_engine_(
          new UpdateEngine(config,
                           update_checker_factory,
                           crx_downloader_factory,
                           ping_manager_.get(),
                           base::Bind(&UpdateClientImpl::NotifyObservers,
                                      base::Unretained(this)))) {
}

UpdateClientImpl::~UpdateClientImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  config_ = nullptr;
}

void UpdateClientImpl::Install(const std::string& id,
                               const CrxDataCallback& crx_data_callback,
                               const CompletionCallback& completion_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (update_engine_->IsUpdating(id)) {
    completion_callback.Run(Error::ERROR_UPDATE_IN_PROGRESS);
    return;
  }

  std::vector<std::string> ids;
  ids.push_back(id);

  // Partially applies |completion_callback| to OnTaskComplete, so this
  // argument is available when the task completes, along with the task itself.
  const auto callback =
      base::Bind(&UpdateClientImpl::OnTaskComplete, this, completion_callback);
  scoped_ptr<TaskUpdate> task(new TaskUpdate(update_engine_.get(), true, ids,
                                             crx_data_callback, callback));

  auto it = tasks_.insert(task.release()).first;
  RunTask(*it);
}

void UpdateClientImpl::Update(const std::vector<std::string>& ids,
                              const CrxDataCallback& crx_data_callback,
                              const CompletionCallback& completion_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto callback =
      base::Bind(&UpdateClientImpl::OnTaskComplete, this, completion_callback);
  scoped_ptr<TaskUpdate> task(new TaskUpdate(update_engine_.get(), false, ids,
                                             crx_data_callback, callback));

  if (tasks_.empty()) {
    auto it = tasks_.insert(task.release()).first;
    RunTask(*it);
  } else {
    task_queue_.push(task.release());
  }
}

void UpdateClientImpl::RunTask(Task* task) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&Task::Run, base::Unretained(task)));
}

void UpdateClientImpl::OnTaskComplete(
    const CompletionCallback& completion_callback,
    Task* task,
    int error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(task);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(completion_callback, error));

  tasks_.erase(task);
  delete task;

  if (!task_queue_.empty()) {
    RunTask(task_queue_.front());
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
  FOR_EACH_OBSERVER(Observer, observer_list_, OnEvent(event, id));
}

bool UpdateClientImpl::GetCrxUpdateState(const std::string& id,
                                         CrxUpdateItem* update_item) const {
  return update_engine_->GetUpdateState(id, update_item);
}

bool UpdateClientImpl::IsUpdating(const std::string& id) const {
  return update_engine_->IsUpdating(id);
}

scoped_refptr<UpdateClient> UpdateClientFactory(
    const scoped_refptr<Configurator>& config) {
  scoped_ptr<PingManager> ping_manager(new PingManager(*config));
  return new UpdateClientImpl(config, ping_manager.Pass(),
                              &UpdateChecker::Create, &CrxDownloader::Create);
}

}  // namespace update_client
