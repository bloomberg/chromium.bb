// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/video/gpu_arc_video_service_host.h"

#include <string>
#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/video_accelerator.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_service_registry.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace arc {

namespace {

void DeprecatedConnectToVideoAcceleratorServiceOnIOThread(
    mojom::VideoAcceleratorServiceClientRequest request) {
  // Note |request| is not a ServiceRequest. It is a ClientRequest but doesn't
  // request for a Client. Instead, it requests for a Service while specified
  // the client. It works this odd way because the interfaces were modeled as
  // arc's "Host notifies to Instance::Init", not mojo's typical "Client
  // registers to Service".
  // TODO(kcwu): revise the interface.
  content::GetGpuRemoteInterfaces()->GetInterface(std::move(request));
}

void ConnectToVideoAcceleratorServiceOnIOThread(
    mojom::VideoAcceleratorServiceRequest request) {
  content::GetGpuRemoteInterfaces()->GetInterface(std::move(request));
}

}  // namespace

class VideoAcceleratorFactoryService : public mojom::VideoAcceleratorFactory {
 public:
  VideoAcceleratorFactoryService() = default;

  void Create(mojom::VideoAcceleratorServiceRequest request) override {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&ConnectToVideoAcceleratorServiceOnIOThread,
                   base::Passed(&request)));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoAcceleratorFactoryService);
};

GpuArcVideoServiceHost::GpuArcVideoServiceHost(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service()->video()->AddObserver(this);
}

GpuArcVideoServiceHost::~GpuArcVideoServiceHost() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service()->video()->RemoveObserver(this);
}

void GpuArcVideoServiceHost::OnInstanceReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* video_instance =
      arc_bridge_service()->video()->GetInstanceForMethod("Init");
  DCHECK(video_instance);
  video_instance->Init(binding_.CreateInterfacePtrAndBind());
}

void GpuArcVideoServiceHost::DeprecatedOnRequestArcVideoAcceleratorChannel(
    const DeprecatedOnRequestArcVideoAcceleratorChannelCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Hardcode pid 0 since it is unused in mojo.
  const base::ProcessHandle kUnusedChildProcessHandle =
      base::kNullProcessHandle;
  mojo::edk::PlatformChannelPair channel_pair;
  std::string child_token = mojo::edk::GenerateRandomToken();
  mojo::edk::ChildProcessLaunched(kUnusedChildProcessHandle,
                                  channel_pair.PassServerHandle(), child_token);

  MojoHandle wrapped_handle;
  MojoResult wrap_result = mojo::edk::CreatePlatformHandleWrapper(
      channel_pair.PassClientHandle(), &wrapped_handle);
  if (wrap_result != MOJO_RESULT_OK) {
    LOG(ERROR) << "Pipe failed to wrap handles. Closing: " << wrap_result;
    callback.Run(mojo::ScopedHandle(), std::string());
    return;
  }
  mojo::ScopedHandle child_handle{mojo::Handle(wrapped_handle)};

  std::string token = mojo::edk::GenerateRandomToken();
  mojo::ScopedMessagePipeHandle server_pipe =
      mojo::edk::CreateParentMessagePipe(token, child_token);
  if (!server_pipe.is_valid()) {
    LOG(ERROR) << "Invalid pipe";
    callback.Run(mojo::ScopedHandle(), std::string());
    return;
  }
  callback.Run(std::move(child_handle), token);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(
          &DeprecatedConnectToVideoAcceleratorServiceOnIOThread,
          base::Passed(mojo::MakeRequest<mojom::VideoAcceleratorServiceClient>(
              std::move(server_pipe)))));
}

void GpuArcVideoServiceHost::OnBootstrapVideoAcceleratorFactory(
    const OnBootstrapVideoAcceleratorFactoryCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Hardcode pid 0 since it is unused in mojo.
  const base::ProcessHandle kUnusedChildProcessHandle =
      base::kNullProcessHandle;
  mojo::edk::PlatformChannelPair channel_pair;
  std::string child_token = mojo::edk::GenerateRandomToken();
  mojo::edk::ChildProcessLaunched(kUnusedChildProcessHandle,
                                  channel_pair.PassServerHandle(), child_token);

  MojoHandle wrapped_handle;
  MojoResult wrap_result = mojo::edk::CreatePlatformHandleWrapper(
      channel_pair.PassClientHandle(), &wrapped_handle);
  if (wrap_result != MOJO_RESULT_OK) {
    LOG(ERROR) << "Pipe failed to wrap handles. Closing: " << wrap_result;
    callback.Run(mojo::ScopedHandle(), std::string());
    return;
  }
  mojo::ScopedHandle child_handle{mojo::Handle(wrapped_handle)};

  std::string token = mojo::edk::GenerateRandomToken();
  mojo::ScopedMessagePipeHandle server_pipe =
      mojo::edk::CreateParentMessagePipe(token, child_token);
  if (!server_pipe.is_valid()) {
    LOG(ERROR) << "Invalid pipe";
    callback.Run(mojo::ScopedHandle(), std::string());
    return;
  }
  callback.Run(std::move(child_handle), token);

  mojo::MakeStrongBinding(base::MakeUnique<VideoAcceleratorFactoryService>(),
                          mojo::MakeRequest<mojom::VideoAcceleratorFactory>(
                              std::move(server_pipe)));
}

}  // namespace arc
