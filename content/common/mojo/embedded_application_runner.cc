// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo/embedded_application_runner.h"

#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "services/shell/public/cpp/shell_connection.h"

namespace content {

class EmbeddedApplicationRunner::Instance
    : public base::RefCountedThreadSafe<Instance> {
 public:
  explicit Instance(
      const EmbeddedApplicationRunner::FactoryCallback& callback)
      : factory_callback_(callback) {
    // This object may be used exclusively from a single thread which may be
    // different from the one that created it.
    thread_checker_.DetachFromThread();
  }

  void BindShellClientRequest(mojo::shell::mojom::ShellClientRequest request) {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (!shell_client_)
      shell_client_ = factory_callback_.Run();

    std::unique_ptr<mojo::ShellConnection> new_connection(
        new mojo::ShellConnection(shell_client_.get(), std::move(request)));
    new_connection->set_connection_lost_closure(
        base::Bind(&Instance::OnShellConnectionLost,
                   base::Unretained(this), new_connection.get()));
    shell_connections_.push_back(std::move(new_connection));
  }

 private:
  friend class base::RefCountedThreadSafe<Instance>;

  ~Instance() { DCHECK(thread_checker_.CalledOnValidThread()); }

  void OnShellConnectionLost(mojo::ShellConnection* connection) {
    DCHECK(thread_checker_.CalledOnValidThread());

    for (auto it = shell_connections_.begin(); it != shell_connections_.end();
         ++it) {
      if (it->get() == connection) {
        shell_connections_.erase(it);
        break;
      }
    }

    if (shell_connections_.empty())
      shell_client_.reset();
  }

  base::ThreadChecker thread_checker_;
  const FactoryCallback factory_callback_;
  std::unique_ptr<mojo::ShellClient> shell_client_;
  std::vector<std::unique_ptr<mojo::ShellConnection>> shell_connections_;

  DISALLOW_COPY_AND_ASSIGN(Instance);
};

EmbeddedApplicationRunner::EmbeddedApplicationRunner(
    const FactoryCallback& callback,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : application_task_runner_(
          task_runner ? task_runner : base::ThreadTaskRunnerHandle::Get()),
      instance_(new Instance(callback)) {
}

EmbeddedApplicationRunner::~EmbeddedApplicationRunner() {
}

void EmbeddedApplicationRunner::BindShellClientRequest(
    mojo::shell::mojom::ShellClientRequest request) {
  application_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Instance::BindShellClientRequest, instance_,
                 base::Passed(&request)));
}

}  // namespace content
