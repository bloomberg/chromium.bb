// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo/embedded_application_runner.h"

#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "services/shell/public/cpp/shell_connection.h"

namespace content {

class EmbeddedApplicationRunner::Instance
    : public base::RefCountedThreadSafe<Instance> {
 public:
  explicit Instance(
      const EmbeddedApplicationRunner::FactoryCallback& callback,
      const base::Closure& quit_closure)
      : factory_callback_(callback),
        quit_closure_(quit_closure),
        quit_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
    // This object may be used exclusively from a single thread which may be
    // different from the one that created it.
    thread_checker_.DetachFromThread();
  }

  void BindShellClientRequest(shell::mojom::ShellClientRequest request) {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (!shell_client_) {
      shell_client_ = factory_callback_.Run(
          base::Bind(&Instance::Quit, base::Unretained(this)));
    }

    shell::ShellConnection* new_connection =
        new shell::ShellConnection(shell_client_.get(), std::move(request));
    shell_connections_.push_back(base::WrapUnique(new_connection));
    new_connection->SetConnectionLostClosure(
        base::Bind(&Instance::OnShellConnectionLost, base::Unretained(this),
                   new_connection));
  }

 private:
  friend class base::RefCountedThreadSafe<Instance>;

  ~Instance() { DCHECK(thread_checker_.CalledOnValidThread()); }

  void OnShellConnectionLost(shell::ShellConnection* connection) {
    DCHECK(thread_checker_.CalledOnValidThread());

    for (auto it = shell_connections_.begin(); it != shell_connections_.end();
         ++it) {
      if (it->get() == connection) {
        shell_connections_.erase(it);
        break;
      }
    }
  }

  void Quit() {
    shell_client_.reset();
    quit_task_runner_->PostTask(FROM_HERE, quit_closure_);
  }

  base::ThreadChecker thread_checker_;
  const FactoryCallback factory_callback_;
  std::unique_ptr<shell::ShellClient> shell_client_;
  std::vector<std::unique_ptr<shell::ShellConnection>> shell_connections_;
  const base::Closure quit_closure_;
  const scoped_refptr<base::SingleThreadTaskRunner> quit_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(Instance);
};

EmbeddedApplicationRunner::EmbeddedApplicationRunner(
    const FactoryCallback& callback,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : application_task_runner_(
          task_runner ? task_runner : base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
  instance_ = new Instance(callback,
                           base::Bind(&EmbeddedApplicationRunner::OnQuit,
                                      weak_factory_.GetWeakPtr()));
}

EmbeddedApplicationRunner::~EmbeddedApplicationRunner() {
}

void EmbeddedApplicationRunner::BindShellClientRequest(
    shell::mojom::ShellClientRequest request) {
  application_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Instance::BindShellClientRequest, instance_,
                 base::Passed(&request)));
}

void EmbeddedApplicationRunner::SetQuitClosure(
    const base::Closure& quit_closure) {
  quit_closure_ = quit_closure;
}

void EmbeddedApplicationRunner::OnQuit() {
  quit_closure_.Run();
}

}  // namespace content
