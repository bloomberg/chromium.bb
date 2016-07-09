// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/mojo_child_connection.h"

#include <stdint.h>
#include <utility>

#include "base/macros.h"
#include "content/public/common/mojo_shell_connection.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/interfaces/service.mojom.h"

namespace content {

namespace {

void CallBinderOnTaskRunner(
    const shell::InterfaceRegistry::Binder& binder,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const mojo::String& interface_name,
    mojo::ScopedMessagePipeHandle request_handle) {
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(binder, interface_name, base::Passed(&request_handle)));
}

}  // namespace

class MojoChildConnection::IOThreadContext
    : public base::RefCountedThreadSafe<IOThreadContext> {
 public:
  IOThreadContext() {}

  void Initialize(const std::string& application_name,
                  const std::string& instance_id,
                  shell::Connector* connector,
                  mojo::ScopedMessagePipeHandle service_pipe,
                  scoped_refptr<base::SequencedTaskRunner> io_task_runner,
                  const shell::InterfaceRegistry::Binder& default_binder) {
    DCHECK(!io_task_runner_);
    io_task_runner_ = io_task_runner;
    std::unique_ptr<shell::Connector> io_thread_connector;
    if (connector)
      io_thread_connector = connector->Clone();
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&IOThreadContext::InitializeOnIOThread, this,
                   application_name, instance_id,
                   base::Passed(&io_thread_connector),
                   base::Passed(&service_pipe),
                   base::Bind(&CallBinderOnTaskRunner, default_binder,
                              base::ThreadTaskRunnerHandle::Get())));
  }

  void ShutDown() {
    if (!io_task_runner_)
      return;
    bool posted = io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&IOThreadContext::ShutDownOnIOThread, this));
    DCHECK(posted);
  }

  void GetRemoteInterfaceOnIOThread(
      const mojo::String& interface_name,
      mojo::ScopedMessagePipeHandle request_handle) {
    if (connection_) {
      connection_->GetRemoteInterfaces()->GetInterface(
          interface_name, std::move(request_handle));
    }
  }

  void SetProcessHandle(base::ProcessHandle handle) {
    DCHECK(io_task_runner_);
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&IOThreadContext::SetProcessHandleOnIOThread, this, handle));
  }

 private:
  friend class base::RefCountedThreadSafe<IOThreadContext>;

  virtual ~IOThreadContext() {}

  void InitializeOnIOThread(
      const std::string& application_name,
      const std::string& instance_id,
      std::unique_ptr<shell::Connector> connector,
      mojo::ScopedMessagePipeHandle service_pipe,
      const shell::InterfaceRegistry::Binder& default_binder) {
    shell::mojom::ServicePtr service;
    service.Bind(mojo::InterfacePtrInfo<shell::mojom::Service>(
        std::move(service_pipe), 0u));
    shell::mojom::PIDReceiverRequest pid_receiver_request =
        mojo::GetProxy(&pid_receiver_);

    shell::Identity target(application_name, shell::mojom::kInheritUserID,
                           instance_id);
    shell::Connector::ConnectParams params(target);
    params.set_client_process_connection(std::move(service),
                                         std::move(pid_receiver_request));

    // In some unit testing scenarios a null connector is passed.
    if (!connector)
      return;

    connection_ = connector->Connect(&params);
    connection_->GetInterfaceRegistry()->set_default_binder(default_binder);
  }

  void ShutDownOnIOThread() {
    connection_.reset();
    pid_receiver_.reset();
  }

  void SetProcessHandleOnIOThread(base::ProcessHandle handle) {
    DCHECK(pid_receiver_.is_bound());
    pid_receiver_->SetPID(base::GetProcId(handle));
    pid_receiver_.reset();
  }

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  std::unique_ptr<shell::Connection> connection_;
  shell::mojom::PIDReceiverPtr pid_receiver_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadContext);
};

MojoChildConnection::MojoChildConnection(
    const std::string& application_name,
    const std::string& instance_id,
    const std::string& child_token,
    shell::Connector* connector,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : context_(new IOThreadContext),
      service_token_(mojo::edk::GenerateRandomToken()),
      interface_registry_(nullptr),
      weak_factory_(this) {
  mojo::ScopedMessagePipeHandle service_pipe =
      mojo::edk::CreateParentMessagePipe(service_token_, child_token);

  context_ = new IOThreadContext;
  context_->Initialize(
      application_name, instance_id, connector, std::move(service_pipe),
      io_task_runner,
      base::Bind(&MojoChildConnection::GetInterface,
                 weak_factory_.GetWeakPtr()));
  remote_interfaces_.Forward(
      base::Bind(&CallBinderOnTaskRunner,
                 base::Bind(&IOThreadContext::GetRemoteInterfaceOnIOThread,
                            context_), io_task_runner));

#if defined(OS_ANDROID)
  service_registry_android_ =
      ServiceRegistryAndroid::Create(&interface_registry_, &remote_interfaces_);
#endif
}

MojoChildConnection::~MojoChildConnection() {
  context_->ShutDown();
}

void MojoChildConnection::SetProcessHandle(base::ProcessHandle handle) {
  context_->SetProcessHandle(handle);
}

void MojoChildConnection::GetInterface(
    const mojo::String& interface_name,
    mojo::ScopedMessagePipeHandle request_handle) {
  static_cast<shell::mojom::InterfaceProvider*>(&interface_registry_)
      ->GetInterface(interface_name, std::move(request_handle));
}

}  // namespace content
