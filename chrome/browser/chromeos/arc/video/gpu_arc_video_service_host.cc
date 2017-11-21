// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/video/gpu_arc_video_service_host.h"

#include <memory>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/ash_config.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/common/video_decode_accelerator.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_service_registry.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/outgoing_broker_client_invitation.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/ui/public/cpp/gpu/gpu.h"
#include "ui/aura/env.h"

namespace arc {

namespace {

// Singleton factory for GpuArcVideoServiceHost.
class GpuArcVideoServiceHostFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          GpuArcVideoServiceHost,
          GpuArcVideoServiceHostFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "GpuArcVideoServiceHostFactory";

  static GpuArcVideoServiceHostFactory* GetInstance() {
    return base::Singleton<GpuArcVideoServiceHostFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<GpuArcVideoServiceHostFactory>;
  GpuArcVideoServiceHostFactory() = default;
  ~GpuArcVideoServiceHostFactory() override = default;
};

}  // namespace

class VideoAcceleratorFactoryService : public mojom::VideoAcceleratorFactory {
 public:
  VideoAcceleratorFactoryService() = default;

  void CreateDecodeAccelerator(
      mojom::VideoDecodeAcceleratorRequest request) override {
    if (chromeos::GetAshConfig() != ash::Config::CLASSIC) {
      aura::Env::GetInstance()
          ->GetGpuConnection()
          ->CreateArcVideoDecodeAccelerator(std::move(request));
    } else {
      content::BrowserThread::PostTask(
          content::BrowserThread::IO, FROM_HERE,
          base::BindOnce(&content::BindInterfaceInGpuProcess<
                             mojom::VideoDecodeAccelerator>,
                         base::Passed(&request)));
    }
  }

  void CreateEncodeAccelerator(
      mojom::VideoEncodeAcceleratorRequest request) override {
    if (chromeos::GetAshConfig() != ash::Config::CLASSIC) {
      aura::Env::GetInstance()
          ->GetGpuConnection()
          ->CreateArcVideoEncodeAccelerator(std::move(request));
    } else {
      content::BrowserThread::PostTask(
          content::BrowserThread::IO, FROM_HERE,
          base::BindOnce(&content::BindInterfaceInGpuProcess<
                             mojom::VideoEncodeAccelerator>,
                         base::Passed(&request)));
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoAcceleratorFactoryService);
};

// static
GpuArcVideoServiceHost* GpuArcVideoServiceHost::GetForBrowserContext(
    content::BrowserContext* context) {
  return GpuArcVideoServiceHostFactory::GetForBrowserContext(context);
}

GpuArcVideoServiceHost::GpuArcVideoServiceHost(content::BrowserContext* context,
                                               ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service), binding_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->video()->AddObserver(this);
}

GpuArcVideoServiceHost::~GpuArcVideoServiceHost() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->video()->RemoveObserver(this);
}

void GpuArcVideoServiceHost::OnConnectionReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* video_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->video(), Init);
  DCHECK(video_instance);
  mojom::VideoHostPtr host_proxy;
  binding_.Bind(mojo::MakeRequest(&host_proxy));
  video_instance->Init(std::move(host_proxy));
}

void GpuArcVideoServiceHost::OnBootstrapVideoAcceleratorFactory(
    OnBootstrapVideoAcceleratorFactoryCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Hardcode pid 0 since it is unused in mojo.
  const base::ProcessHandle kUnusedChildProcessHandle =
      base::kNullProcessHandle;
  mojo::edk::OutgoingBrokerClientInvitation invitation;
  mojo::edk::PlatformChannelPair channel_pair;
  std::string token = mojo::edk::GenerateRandomToken();
  mojo::ScopedMessagePipeHandle server_pipe =
      invitation.AttachMessagePipe(token);
  invitation.Send(
      kUnusedChildProcessHandle,
      mojo::edk::ConnectionParams(mojo::edk::TransportProtocol::kLegacy,
                                  channel_pair.PassServerHandle()));

  MojoHandle wrapped_handle;
  MojoResult wrap_result = mojo::edk::CreatePlatformHandleWrapper(
      channel_pair.PassClientHandle(), &wrapped_handle);
  if (wrap_result != MOJO_RESULT_OK) {
    LOG(ERROR) << "Pipe failed to wrap handles. Closing: " << wrap_result;
    std::move(callback).Run(mojo::ScopedHandle(), std::string());
    return;
  }
  mojo::ScopedHandle child_handle{mojo::Handle(wrapped_handle)};

  std::move(callback).Run(std::move(child_handle), token);

  mojo::MakeStrongBinding(
      std::make_unique<VideoAcceleratorFactoryService>(),
      mojom::VideoAcceleratorFactoryRequest(std::move(server_pipe)));
}

}  // namespace arc
