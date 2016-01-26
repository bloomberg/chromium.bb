// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/mojo_application_host.h"

#include <utility>

#include "build/build_config.h"
#include "content/common/mojo/mojo_messages.h"
#include "content/public/browser/browser_thread.h"
#include "ipc/ipc_sender.h"
#include "third_party/mojo/src/mojo/edk/embedder/platform_channel_pair.h"

namespace content {
namespace {

base::PlatformFile PlatformFileFromScopedPlatformHandle(
    mojo::embedder::ScopedPlatformHandle handle) {
#if defined(OS_POSIX)
  return handle.release().fd;
#elif defined(OS_WIN)
  return handle.release().handle;
#endif
}

class ApplicationSetupImpl : public ApplicationSetup {
 public:
  ApplicationSetupImpl(ServiceRegistryImpl* service_registry,
                       mojo::InterfaceRequest<ApplicationSetup> request)
      : binding_(this, std::move(request)),
        service_registry_(service_registry) {}

  ~ApplicationSetupImpl() override {
  }

 private:
  // ApplicationSetup implementation.
  void ExchangeServiceProviders(
      mojo::InterfaceRequest<mojo::ServiceProvider> services,
      mojo::ServiceProviderPtr exposed_services) override {
    service_registry_->Bind(std::move(services));
    service_registry_->BindRemoteServiceProvider(std::move(exposed_services));
  }

  mojo::Binding<ApplicationSetup> binding_;
  ServiceRegistryImpl* service_registry_;
};

}  // namespace

MojoApplicationHost::MojoApplicationHost()
    : did_activate_(false), weak_factory_(this) {
#if defined(OS_ANDROID)
  service_registry_android_.reset(
      new ServiceRegistryAndroid(&service_registry_));
#endif
}

MojoApplicationHost::~MojoApplicationHost() {
}

bool MojoApplicationHost::Init() {
  DCHECK(!client_handle_.is_valid()) << "Already initialized!";

  mojo::embedder::PlatformChannelPair channel_pair;

  scoped_refptr<base::TaskRunner> io_task_runner;
  if (io_task_runner_override_) {
    io_task_runner = io_task_runner_override_;
  } else {
    io_task_runner =
        BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::IO)
          ->task_runner();
  }

  // Forward this to the client once we know its process handle.
  client_handle_ = channel_pair.PassClientHandle();

  channel_init_.Init(
      PlatformFileFromScopedPlatformHandle(channel_pair.PassServerHandle()),
      io_task_runner,
      base::Bind(&MojoApplicationHost::OnMessagePipeCreated,
                 weak_factory_.GetWeakPtr()));

  return true;
}

void MojoApplicationHost::Activate(IPC::Sender* sender,
                                   base::ProcessHandle process_handle) {
  DCHECK(!did_activate_);
  DCHECK(client_handle_.is_valid());

  base::PlatformFile client_file =
      PlatformFileFromScopedPlatformHandle(std::move(client_handle_));
  did_activate_ = sender->Send(new MojoMsg_Activate(
      IPC::GetFileHandleForProcess(client_file, process_handle, true)));
}

void MojoApplicationHost::WillDestroySoon() {
  channel_init_.WillDestroySoon();
}

void MojoApplicationHost::OverrideIOTaskRunnerForTest(
    scoped_refptr<base::TaskRunner> io_task_runner) {
  io_task_runner_override_ = io_task_runner;
}

void MojoApplicationHost::OnMessagePipeCreated(
    mojo::ScopedMessagePipeHandle pipe) {
  DCHECK(pipe.is_valid());
  application_setup_.reset(new ApplicationSetupImpl(
      &service_registry_,
      mojo::MakeRequest<ApplicationSetup>(std::move(pipe))));
}

}  // namespace content
