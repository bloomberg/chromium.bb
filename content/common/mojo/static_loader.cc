// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo/static_loader.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/simple_thread.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"

namespace content {

namespace {

class RunnerThread : public base::SimpleThread {
 public:
  RunnerThread(const std::string& name,
               shell::mojom::ShellClientRequest request,
               scoped_refptr<base::TaskRunner> exit_task_runner,
               const base::Closure& exit_callback,
               const StaticLoader::ApplicationFactory& factory)
      : base::SimpleThread("Mojo Application: " + name),
        request_(std::move(request)),
        exit_task_runner_(exit_task_runner),
        exit_callback_(exit_callback),
        factory_(factory) {}

  void Run() override {
    std::unique_ptr<shell::ApplicationRunner> runner(
        new shell::ApplicationRunner(factory_.Run().release()));
    runner->Run(request_.PassMessagePipe().release().value(),
                false /* init_base */);
    exit_task_runner_->PostTask(FROM_HERE, exit_callback_);
  }

 private:
  shell::mojom::ShellClientRequest request_;
  scoped_refptr<base::TaskRunner> exit_task_runner_;
  base::Closure exit_callback_;
  StaticLoader::ApplicationFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(RunnerThread);
};

}  // namespace

StaticLoader::StaticLoader(const ApplicationFactory& factory)
    : StaticLoader(factory, base::Closure()) {
}

StaticLoader::StaticLoader(const ApplicationFactory& factory,
                           const base::Closure& quit_callback)
    : factory_(factory), quit_callback_(quit_callback), weak_factory_(this) {
}

StaticLoader::~StaticLoader() {
  if (thread_)
    StopAppThread();
}

void StaticLoader::Load(const std::string& name,
                        shell::mojom::ShellClientRequest request) {
  if (thread_)
    return;

  // If the application's thread quits on its own before this loader dies, we
  // reset the Thread object, allowing future Load requests to be fulfilled
  // with a new app instance.
  auto exit_callback = base::Bind(&StaticLoader::StopAppThread,
                                  weak_factory_.GetWeakPtr());
  thread_.reset(new RunnerThread(name, std::move(request),
                                 base::ThreadTaskRunnerHandle::Get(),
                                 exit_callback, factory_));
  thread_->Start();
}

void StaticLoader::StopAppThread() {
  thread_->Join();
  thread_.reset();
  if (!quit_callback_.is_null())
    quit_callback_.Run();
}

}  // namespace content
