// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/static_application_loader.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/simple_thread.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/interfaces/shell_client.mojom.h"

namespace mojo {
namespace shell {

namespace {

class RunnerThread : public base::SimpleThread {
 public:
  RunnerThread(const GURL& url,
               InterfaceRequest<mojom::ShellClient> request,
               scoped_refptr<base::TaskRunner> exit_task_runner,
               const base::Closure& exit_callback,
               const StaticApplicationLoader::ApplicationFactory& factory)
      : base::SimpleThread("Mojo Application: " + url.spec()),
        request_(std::move(request)),
        exit_task_runner_(exit_task_runner),
        exit_callback_(exit_callback),
        factory_(factory) {}

  void Run() override {
    scoped_ptr<ApplicationRunner> runner(
        new ApplicationRunner(factory_.Run().release()));
    runner->Run(request_.PassMessagePipe().release().value(),
                false /* init_base */);
    exit_task_runner_->PostTask(FROM_HERE, exit_callback_);
  }

 private:
  InterfaceRequest<mojom::ShellClient> request_;
  scoped_refptr<base::TaskRunner> exit_task_runner_;
  base::Closure exit_callback_;
  StaticApplicationLoader::ApplicationFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(RunnerThread);
};

}  // namespace

StaticApplicationLoader::StaticApplicationLoader(
    const ApplicationFactory& factory)
    : StaticApplicationLoader(factory, base::Closure()) {
}

StaticApplicationLoader::StaticApplicationLoader(
    const ApplicationFactory& factory,
    const base::Closure& quit_callback)
    : factory_(factory), quit_callback_(quit_callback), weak_factory_(this) {
}

StaticApplicationLoader::~StaticApplicationLoader() {
  if (thread_)
    StopAppThread();
}

void StaticApplicationLoader::Load(
    const GURL& url,
    InterfaceRequest<mojom::ShellClient> request) {
  if (thread_)
    return;

  // If the application's thread quits on its own before this loader dies, we
  // reset the Thread object, allowing future Load requests to be fulfilled
  // with a new app instance.
  auto exit_callback = base::Bind(&StaticApplicationLoader::StopAppThread,
                                  weak_factory_.GetWeakPtr());
  thread_.reset(new RunnerThread(url, std::move(request),
                                 base::ThreadTaskRunnerHandle::Get(),
                                 exit_callback, factory_));
  thread_->Start();
}

void StaticApplicationLoader::StopAppThread() {
  thread_->Join();
  thread_.reset();
  if (!quit_callback_.is_null())
    quit_callback_.Run();
}

}  // namespace shell
}  // namespace mojo
