// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/updater.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/initialization_util.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/updater/configurator.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "mojo/core/embedder/embedder.h"

namespace updater {

namespace {

const uint8_t jebg_hash[] = {0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec,
                             0x8e, 0xd5, 0xfa, 0x54, 0xb0, 0xd2, 0xdd, 0xa5,
                             0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47, 0xf6, 0xc4,
                             0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40};

void TaskSchedulerStart() {
  base::TaskScheduler::Create("Updater");
  const auto task_scheduler_init_params =
      std::make_unique<base::TaskScheduler::InitParams>(
          base::SchedulerWorkerPoolParams(
              base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.1, 0),
              base::TimeDelta::FromSeconds(30)),
          base::SchedulerWorkerPoolParams(
              base::RecommendedMaxNumberOfThreadsInPool(8, 32, 0.3, 0),
              base::TimeDelta::FromSeconds(30)));
  base::TaskScheduler::GetInstance()->Start(*task_scheduler_init_params);
}

void TaskSchedulerStop() {
  base::TaskScheduler::GetInstance()->Shutdown();
}

void QuitLoop(base::OnceClosure quit_closure) {
  std::move(quit_closure).Run();
}

class Observer : public update_client::UpdateClient::Observer {
 public:
  explicit Observer(scoped_refptr<update_client::UpdateClient> update_client)
      : update_client_(update_client) {}

  // Overrides for update_client::UpdateClient::Observer.
  void OnEvent(Events event, const std::string& id) override {
    update_client::CrxUpdateItem item;
    update_client_->GetCrxUpdateState(id, &item);
  }

 private:
  scoped_refptr<update_client::UpdateClient> update_client_;
  DISALLOW_COPY_AND_ASSIGN(Observer);
};

}  // namespace

int UpdaterMain(int argc, const char* const* argv) {
  base::CommandLine::Init(argc, argv);

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch("test"))
    return 0;

  base::AtExitManager exit_manager;

  mojo::core::Init();

  TaskSchedulerStart();

  base::MessageLoopForUI message_loop;
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());
  base::PlatformThread::SetName("UpdaterMain");

  base::RunLoop runloop;

  auto config = base::MakeRefCounted<Configurator>();
  auto update_client = update_client::UpdateClientFactory(config);

  Observer observer(update_client);
  update_client->AddObserver(&observer);

  const std::vector<std::string> ids = {"jebgalgnebhfojomionfpkfelancnnkf"};
  update_client->Update(
      ids,
      base::BindOnce(
          [](const std::vector<std::string>& ids)
              -> std::vector<base::Optional<update_client::CrxComponent>> {
            update_client::CrxComponent component;
            component.name = "jebg";
            component.pk_hash.assign(jebg_hash,
                                     jebg_hash + base::size(jebg_hash));
            component.version = base::Version("0.0");
            return {component};
          }),
      true,
      base::BindOnce(
          [](base::OnceClosure closure, update_client::Error error) {
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, base::BindOnce(&QuitLoop, std::move(closure)));
          },
          runloop.QuitClosure()));

  runloop.Run();

  update_client->RemoveObserver(&observer);
  update_client = nullptr;
  TaskSchedulerStop();
  return 0;
}

}  // namespace updater
