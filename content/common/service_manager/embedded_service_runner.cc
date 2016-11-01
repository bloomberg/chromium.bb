// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_manager/embedded_service_runner.h"

#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace content {

class EmbeddedServiceRunner::Instance
    : public base::RefCountedThreadSafe<Instance> {
 public:
  Instance(const base::StringPiece& name,
           const ServiceInfo& info,
           const base::Closure& quit_closure)
      : name_(name.as_string()),
        factory_callback_(info.factory),
        use_own_thread_(!info.task_runner && info.use_own_thread),
        service_owns_context_(info.service_owns_context),
        quit_closure_(quit_closure),
        quit_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        task_runner_(info.task_runner) {
    if (!use_own_thread_ && !task_runner_)
      task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }

  void BindServiceRequest(service_manager::mojom::ServiceRequest request) {
    DCHECK(runner_thread_checker_.CalledOnValidThread());

    if (use_own_thread_ && !thread_) {
      // Start a new thread if necessary.
      thread_.reset(new base::Thread(name_));
      thread_->Start();
      task_runner_ = thread_->task_runner();
    }

    DCHECK(task_runner_);
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Instance::BindServiceRequestOnApplicationThread, this,
                   base::Passed(&request)));
  }

  void ShutDown() {
    DCHECK(runner_thread_checker_.CalledOnValidThread());
    if (!task_runner_)
      return;
    // Any extant ServiceContexts must be destroyed on the application thread.
    if (task_runner_->BelongsToCurrentThread()) {
      Quit();
    } else {
      task_runner_->PostTask(FROM_HERE,
                                         base::Bind(&Instance::Quit, this));
    }
  }

 private:
  friend class base::RefCountedThreadSafe<Instance>;

  ~Instance() {
    // If this instance had its own thread, it MUST be explicitly destroyed by
    // QuitOnRunnerThread() by the time this destructor is run.
    DCHECK(!thread_);
  }

  void BindServiceRequestOnApplicationThread(
      service_manager::mojom::ServiceRequest request) {
    DCHECK(task_runner_->BelongsToCurrentThread());

    if (!service_) {
      service_ = factory_callback_.Run(
          base::Bind(&Instance::Quit, base::Unretained(this)));
    }

    std::unique_ptr<service_manager::ServiceContext> new_connection =
        base::MakeUnique<service_manager::ServiceContext>(
            service_.get(), std::move(request));
    if (service_owns_context_) {
      service_->set_context(std::move(new_connection));
    } else {
      service_manager::ServiceContext* new_connection_ptr =
          new_connection.get();
      service_manager_connections_.push_back(std::move(new_connection));
      new_connection_ptr->SetConnectionLostClosure(
          base::Bind(&Instance::OnStop, base::Unretained(this),
                     new_connection_ptr));
    }
  }

  void OnStop(service_manager::ServiceContext* connection) {
    DCHECK(task_runner_->BelongsToCurrentThread());

    for (auto it = service_manager_connections_.begin();
         it != service_manager_connections_.end(); ++it) {
      if (it->get() == connection) {
        service_manager_connections_.erase(it);
        break;
      }
    }
  }

  void Quit() {
    DCHECK(task_runner_->BelongsToCurrentThread());

    service_manager_connections_.clear();
    service_.reset();
    if (quit_task_runner_->BelongsToCurrentThread()) {
      QuitOnRunnerThread();
    } else {
      quit_task_runner_->PostTask(
          FROM_HERE, base::Bind(&Instance::QuitOnRunnerThread, this));
    }
  }

  void QuitOnRunnerThread() {
    DCHECK(runner_thread_checker_.CalledOnValidThread());
    if (thread_) {
      thread_.reset();
      task_runner_ = nullptr;
    }
    quit_closure_.Run();
  }

  const std::string name_;
  const ServiceInfo::ServiceFactory factory_callback_;
  const bool use_own_thread_;
  const bool service_owns_context_;
  const base::Closure quit_closure_;
  const scoped_refptr<base::SingleThreadTaskRunner> quit_task_runner_;

  // Thread checker used to ensure certain operations happen only on the
  // runner's (i.e. our owner's) thread.
  base::ThreadChecker runner_thread_checker_;

  // These fields must only be accessed from the runner's thread.
  std::unique_ptr<base::Thread> thread_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // These fields must only be accessed from the application thread, except in
  // the destructor which may run on either the runner thread or the application
  // thread.
  std::unique_ptr<service_manager::Service> service_;
  std::vector<std::unique_ptr<service_manager::ServiceContext>>
      service_manager_connections_;

  DISALLOW_COPY_AND_ASSIGN(Instance);
};

EmbeddedServiceRunner::EmbeddedServiceRunner(const base::StringPiece& name,
                                             const ServiceInfo& info)
    : weak_factory_(this) {
  instance_ = new Instance(name, info,
                           base::Bind(&EmbeddedServiceRunner::OnQuit,
                                      weak_factory_.GetWeakPtr()));
}

EmbeddedServiceRunner::~EmbeddedServiceRunner() {
  instance_->ShutDown();
}

void EmbeddedServiceRunner::BindServiceRequest(
    service_manager::mojom::ServiceRequest request) {
  instance_->BindServiceRequest(std::move(request));
}

void EmbeddedServiceRunner::SetQuitClosure(
    const base::Closure& quit_closure) {
  quit_closure_ = quit_closure;
}

void EmbeddedServiceRunner::OnQuit() {
  if (!quit_closure_.is_null())
    quit_closure_.Run();
}

}  // namespace content
