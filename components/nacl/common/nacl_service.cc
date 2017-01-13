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
#include "mojo/edk/embedder/scoped_ipc_support.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_info.h"

#if defined(OS_POSIX)
#include "base/posix/global_descriptors.h"
#include "content/public/common/content_descriptors.h"
#elif defined(OS_WIN)
#include "mojo/edk/embedder/platform_channel_pair.h"
#endif

namespace {

void EstablishMojoConnection() {
#if defined(OS_WIN)
  mojo::edk::ScopedPlatformHandle platform_channel(
      mojo::edk::PlatformChannelPair::PassClientHandleFromParentProcess(
          *base::CommandLine::ForCurrentProcess()));
#else
  mojo::edk::ScopedPlatformHandle platform_channel(mojo::edk::PlatformHandle(
      base::GlobalDescriptors::GetInstance()->Get(kMojoIPCChannel)));
#endif
  DCHECK(platform_channel.is_valid());
  mojo::edk::SetParentPipeHandle(std::move(platform_channel));
}

service_manager::mojom::ServiceRequest ConnectToServiceManager() {
  const std::string service_request_channel_token =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kServiceRequestChannelToken);
  DCHECK(!service_request_channel_token.empty());
  mojo::ScopedMessagePipeHandle parent_handle =
      mojo::edk::CreateChildMessagePipe(service_request_channel_token);
  DCHECK(parent_handle.is_valid());
  return mojo::MakeRequest<service_manager::mojom::Service>(
      std::move(parent_handle));
}

void ConnectBootstrapChannel(IPC::mojom::ChannelBootstrapPtrInfo ptr,
                             IPC::mojom::ChannelBootstrapRequest request) {
  mojo::FuseInterface(std::move(request), std::move(ptr));
}

class NaClService : public service_manager::Service {
 public:
  NaClService(IPC::mojom::ChannelBootstrapPtrInfo bootstrap,
              std::unique_ptr<mojo::edk::ScopedIPCSupport> ipc_support);
  ~NaClService() override;

  // Service overrides.
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

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

bool NaClService::OnConnect(const service_manager::ServiceInfo& remote_info,
                            service_manager::InterfaceRegistry* registry) {
  if (remote_info.identity.name() != content::mojom::kBrowserServiceName)
    return false;

  if (connected_)
    return false;

  connected_ = true;
  registry->AddInterface(base::Bind(&ConnectBootstrapChannel,
                                    base::Passed(&ipc_channel_bootstrap_)));
  return true;
}

}  // namespace

std::unique_ptr<service_manager::ServiceContext> CreateNaClServiceContext(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    mojo::ScopedMessagePipeHandle* ipc_channel) {
  auto ipc_support = base::MakeUnique<mojo::edk::ScopedIPCSupport>(
      std::move(io_task_runner),
      mojo::edk::ScopedIPCSupport::ShutdownPolicy::FAST);
  EstablishMojoConnection();

  IPC::mojom::ChannelBootstrapPtr bootstrap;
  *ipc_channel = mojo::MakeRequest(&bootstrap).PassMessagePipe();
  return base::MakeUnique<service_manager::ServiceContext>(
      base::MakeUnique<NaClService>(bootstrap.PassInterface(),
                                    std::move(ipc_support)),
      ConnectToServiceManager());
}
