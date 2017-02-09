// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/video/gpu_arc_video_service_host.h"

#include <string>
#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/video_accelerator.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_service_registry.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/pending_process_connection.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace arc {

namespace {

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
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service()->video(), Init);
  DCHECK(video_instance);
  video_instance->Init(binding_.CreateInterfacePtrAndBind());
}

void GpuArcVideoServiceHost::OnBootstrapVideoAcceleratorFactory(
    const OnBootstrapVideoAcceleratorFactoryCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Hardcode pid 0 since it is unused in mojo.
  const base::ProcessHandle kUnusedChildProcessHandle =
      base::kNullProcessHandle;
  mojo::edk::PendingProcessConnection process;
  mojo::edk::PlatformChannelPair channel_pair;
  process.Connect(kUnusedChildProcessHandle, channel_pair.PassServerHandle());

  MojoHandle wrapped_handle;
  MojoResult wrap_result = mojo::edk::CreatePlatformHandleWrapper(
      channel_pair.PassClientHandle(), &wrapped_handle);
  if (wrap_result != MOJO_RESULT_OK) {
    LOG(ERROR) << "Pipe failed to wrap handles. Closing: " << wrap_result;
    callback.Run(mojo::ScopedHandle(), std::string());
    return;
  }
  mojo::ScopedHandle child_handle{mojo::Handle(wrapped_handle)};

  std::string token;
  mojo::ScopedMessagePipeHandle server_pipe = process.CreateMessagePipe(&token);
  callback.Run(std::move(child_handle), token);

  mojo::MakeStrongBinding(base::MakeUnique<VideoAcceleratorFactoryService>(),
                          mojo::MakeRequest<mojom::VideoAcceleratorFactory>(
                              std::move(server_pipe)));
}

}  // namespace arc
