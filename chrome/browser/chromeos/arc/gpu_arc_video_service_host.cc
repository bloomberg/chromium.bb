// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/gpu_arc_video_service_host.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_service_registry.h"
#include "content/public/common/service_registry.h"

namespace {

mojo::InterfacePtrInfo<arc::mojom::VideoHost> GetServiceOnIOThread() {
  arc::mojom::VideoHostPtr host_ptr;
  content::ServiceRegistry* registry = content::GetGpuServiceRegistry();
  registry->ConnectToRemoteService(mojo::GetProxy(&host_ptr));

  // Unbind and reply back to UI thread.
  return host_ptr.PassInterface();
}

}  // namespace

namespace arc {

GpuArcVideoServiceHost::GpuArcVideoServiceHost(
    arc::ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this), weak_factory_(this) {
  arc_bridge_service()->AddObserver(this);
}

GpuArcVideoServiceHost::~GpuArcVideoServiceHost() {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_bridge_service()->RemoveObserver(this);
}

void GpuArcVideoServiceHost::OnVideoInstanceReady() {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto video_instance = arc_bridge_service()->video_instance();
  DCHECK(video_instance);
  video_instance->Init(binding_.CreateInterfacePtrAndBind());
}

void GpuArcVideoServiceHost::OnVideoInstanceClosed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  service_ptr_.reset();
}

void GpuArcVideoServiceHost::OnRequestArcVideoAcceleratorChannel(
    const OnRequestArcVideoAcceleratorChannelCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::IO, FROM_HERE, base::Bind(&GetServiceOnIOThread),
      base::Bind(&GpuArcVideoServiceHost::BindServiceAndCreateChannel,
                 weak_factory_.GetWeakPtr(), callback));
}

void GpuArcVideoServiceHost::BindServiceAndCreateChannel(
    const OnRequestArcVideoAcceleratorChannelCallback& callback,
    mojo::InterfacePtrInfo<arc::mojom::VideoHost> ptr_info) {
  DCHECK(thread_checker_.CalledOnValidThread());

  service_ptr_.Bind(std::move(ptr_info));
  service_ptr_->OnRequestArcVideoAcceleratorChannel(callback);
}

}  // namespace arc
