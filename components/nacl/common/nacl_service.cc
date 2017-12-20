// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/nacl_service.h"

#include <string>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "content/public/common/mojo_channel_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "ipc/ipc.mojom.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/incoming_broker_client_invitation.h"
#include "mojo/edk/embedder/scoped_ipc_support.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"

#if defined(OS_POSIX)
#include "base/posix/global_descriptors.h"
#include "content/public/common/content_descriptors.h"
#elif defined(OS_WIN)
#include "mojo/edk/embedder/platform_channel_pair.h"
#endif

namespace {

std::unique_ptr<mojo::edk::IncomingBrokerClientInvitation>
EstablishMojoConnection() {
#if defined(OS_WIN)
  mojo::edk::ScopedPlatformHandle platform_channel(
      mojo::edk::PlatformChannelPair::PassClientHandleFromParentProcess(
          *base::CommandLine::ForCurrentProcess()));
#else
  mojo::edk::ScopedPlatformHandle platform_channel(mojo::edk::PlatformHandle(
      base::GlobalDescriptors::GetInstance()->Get(kMojoIPCChannel)));
#endif
  DCHECK(platform_channel.is_valid());
  return mojo::edk::IncomingBrokerClientInvitation::Accept(
      mojo::edk::ConnectionParams(mojo::edk::TransportProtocol::kLegacy,
                                  std::move(platform_channel)));
}

service_manager::mojom::ServiceRequest ConnectToServiceManager(
    mojo::edk::IncomingBrokerClientInvitation* invitation) {
  const std::string service_request_channel_token =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kServiceRequestChannelToken);
  DCHECK(!service_request_channel_token.empty());
  mojo::ScopedMessagePipeHandle parent_handle =
      invitation->ExtractMessagePipe(service_request_channel_token);
  DCHECK(parent_handle.is_valid());
  return service_manager::mojom::ServiceRequest(std::move(parent_handle));
}

class NaClService : public service_manager::Service {
 public:
  NaClService(IPC::mojom::ChannelBootstrapPtrInfo bootstrap,
              std::unique_ptr<mojo::edk::ScopedIPCSupport> ipc_support);
  ~NaClService() override;

  // Service overrides.
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

 private:
  IPC::mojom::ChannelBootstrapPtrInfo ipc_channel_bootstrap_;
  std::unique_ptr<mojo::edk::ScopedIPCSupport> ipc_support_;
  bool connected_ = false;
};

NaClService::NaClService(
    IPC::mojom::ChannelBootstrapPtrInfo bootstrap,
    std::unique_ptr<mojo::edk::ScopedIPCSupport> ipc_support)
    : ipc_channel_bootstrap_(std::move(bootstrap)),
      ipc_support_(std::move(ipc_support)) {}

NaClService::~NaClService() = default;

void NaClService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (source_info.identity.name() == content::mojom::kBrowserServiceName &&
      interface_name == IPC::mojom::ChannelBootstrap::Name_ && !connected_) {
    connected_ = true;
    mojo::FuseInterface(
        IPC::mojom::ChannelBootstrapRequest(std::move(interface_pipe)),
        std::move(ipc_channel_bootstrap_));
  } else {
    DVLOG(1) << "Ignoring request for unknown interface " << interface_name;
  }
}

}  // namespace

std::unique_ptr<service_manager::ServiceContext> CreateNaClServiceContext(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    mojo::ScopedMessagePipeHandle* ipc_channel) {
  auto ipc_support = base::MakeUnique<mojo::edk::ScopedIPCSupport>(
      std::move(io_task_runner),
      mojo::edk::ScopedIPCSupport::ShutdownPolicy::FAST);
  auto invitation = EstablishMojoConnection();
  IPC::mojom::ChannelBootstrapPtr bootstrap;
  *ipc_channel = mojo::MakeRequest(&bootstrap).PassMessagePipe();
  auto context = base::MakeUnique<service_manager::ServiceContext>(
      base::MakeUnique<NaClService>(bootstrap.PassInterface(),
                                    std::move(ipc_support)),
      ConnectToServiceManager(invitation.get()));
  return context;
}
