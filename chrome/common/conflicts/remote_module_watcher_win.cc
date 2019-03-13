// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/conflicts/remote_module_watcher_win.h"

#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

// Note: Can be called on any threads, even those not owned by the task
//       scheduler.
void OnModuleEvent(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const ModuleWatcher::OnModuleEventCallback& on_module_event_callback,
    const ModuleWatcher::ModuleEvent& event) {
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(on_module_event_callback, event));
}

}  // namespace

RemoteModuleWatcher::~RemoteModuleWatcher() = default;

// static
RemoteModuleWatcher::UniquePtr RemoteModuleWatcher::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    service_manager::Connector* connector) {
  auto remote_module_watcher =
      UniquePtr(new RemoteModuleWatcher(task_runner),
                base::OnTaskRunnerDeleter(task_runner));

  // Because |remote_module_watcher| will be sent for deletion on |task_runner|,
  // using an unretained pointer to it is safe as the initialization is
  // guaranteed to be run before the destructor.
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&RemoteModuleWatcher::InitializeOnTaskRunner,
                                base::Unretained(remote_module_watcher.get()),
                                connector->Clone()));

  return remote_module_watcher;
}

RemoteModuleWatcher::RemoteModuleWatcher(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner), weak_ptr_factory_(this) {}

void RemoteModuleWatcher::InitializeOnTaskRunner(
    std::unique_ptr<service_manager::Connector> connector) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  connector->BindInterface(content::mojom::kBrowserServiceName,
                           &module_event_sink_);

  module_watcher_ = ModuleWatcher::Create(base::BindRepeating(
      &OnModuleEvent, task_runner_,
      base::BindRepeating(&RemoteModuleWatcher::HandleModuleEvent,
                          weak_ptr_factory_.GetWeakPtr())));
}

void RemoteModuleWatcher::HandleModuleEvent(
    const ModuleWatcher::ModuleEvent& event) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  module_event_sink_->OnModuleEvent(
      event.event_type, reinterpret_cast<uintptr_t>(event.module_load_address));
}
