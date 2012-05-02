// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/browser_gpu_channel_host_factory.h"

#include "base/bind.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"

namespace content {

BrowserGpuChannelHostFactory* BrowserGpuChannelHostFactory::instance_ = NULL;

BrowserGpuChannelHostFactory::CreateRequest::CreateRequest()
    : event(false, false),
      route_id(MSG_ROUTING_NONE) {
}

BrowserGpuChannelHostFactory::CreateRequest::~CreateRequest() {
}

BrowserGpuChannelHostFactory::EstablishRequest::EstablishRequest()
    : event(false, false) {
}

BrowserGpuChannelHostFactory::EstablishRequest::~EstablishRequest() {
}

void BrowserGpuChannelHostFactory::Initialize() {
  instance_ = new BrowserGpuChannelHostFactory();
}

void BrowserGpuChannelHostFactory::Terminate() {
  delete instance_;
  instance_ = NULL;
}

BrowserGpuChannelHostFactory::BrowserGpuChannelHostFactory()
    : gpu_client_id_(ChildProcessHostImpl::GenerateChildProcessUniqueId()),
      shutdown_event_(new base::WaitableEvent(true, false)),
      gpu_host_id_(0) {
}

BrowserGpuChannelHostFactory::~BrowserGpuChannelHostFactory() {
  shutdown_event_->Signal();
}

bool BrowserGpuChannelHostFactory::IsMainThread() {
  return BrowserThread::CurrentlyOn(BrowserThread::UI);
}

bool BrowserGpuChannelHostFactory::IsIOThread() {
  return BrowserThread::CurrentlyOn(BrowserThread::IO);
}

MessageLoop* BrowserGpuChannelHostFactory::GetMainLoop() {
  return BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::UI);
}

scoped_refptr<base::MessageLoopProxy>
BrowserGpuChannelHostFactory::GetIOLoopProxy() {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
}

base::WaitableEvent* BrowserGpuChannelHostFactory::GetShutDownEvent() {
  return shutdown_event_.get();
}

scoped_ptr<base::SharedMemory>
BrowserGpuChannelHostFactory::AllocateSharedMemory(uint32 size) {
  scoped_ptr<base::SharedMemory> shm(new base::SharedMemory());
  if (!shm->CreateAnonymous(size))
    return scoped_ptr<base::SharedMemory>();
  return shm.Pass();
}

void BrowserGpuChannelHostFactory::CreateViewCommandBufferOnIO(
    CreateRequest* request,
    int32 surface_id,
    const GPUCreateCommandBufferConfig& init_params) {
  GpuProcessHost* host = GpuProcessHost::FromID(gpu_host_id_);
  if (!host) {
    request->event.Signal();
    return;
  }

  gfx::GLSurfaceHandle surface =
      GpuSurfaceTracker::Get()->GetSurfaceHandle(surface_id);

  host->CreateViewCommandBuffer(
      surface,
      surface_id,
      gpu_client_id_,
      init_params,
      base::Bind(&BrowserGpuChannelHostFactory::CommandBufferCreatedOnIO,
                 request));
}

// static
void BrowserGpuChannelHostFactory::CommandBufferCreatedOnIO(
    CreateRequest* request, int32 route_id) {
  request->route_id = route_id;
  request->event.Signal();
}

int32 BrowserGpuChannelHostFactory::CreateViewCommandBuffer(
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params) {
  CreateRequest request;
  GetIOLoopProxy()->PostTask(FROM_HERE, base::Bind(
        &BrowserGpuChannelHostFactory::CreateViewCommandBufferOnIO,
        base::Unretained(this),
        &request,
        surface_id,
        init_params));
  // We're blocking the UI thread, which is generally undesirable.
  // In this case we need to wait for this before we can show any UI /anyway/,
  // so it won't cause additional jank.
  // TODO(piman): Make this asynchronous (http://crbug.com/125248).
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  request.event.Wait();
  return request.route_id;
}

void BrowserGpuChannelHostFactory::EstablishGpuChannelOnIO(
    EstablishRequest* request,
    CauseForGpuLaunch cause_for_gpu_launch) {
  GpuProcessHost* host = GpuProcessHost::FromID(gpu_host_id_);
  if (!host) {
    host = GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                               cause_for_gpu_launch);
    if (!host) {
      request->event.Signal();
      return;
    }
    gpu_host_id_ = host->host_id();
  }

  host->EstablishGpuChannel(
      gpu_client_id_,
      true,
      base::Bind(&BrowserGpuChannelHostFactory::GpuChannelEstablishedOnIO,
                 request));
}

// static
void BrowserGpuChannelHostFactory::GpuChannelEstablishedOnIO(
    EstablishRequest* request,
    const IPC::ChannelHandle& channel_handle,
    const GPUInfo& gpu_info) {
  request->channel_handle = channel_handle;
  request->gpu_info = gpu_info;
  request->event.Signal();
}

GpuChannelHost* BrowserGpuChannelHostFactory::EstablishGpuChannelSync(
    CauseForGpuLaunch cause_for_gpu_launch) {
  if (gpu_channel_.get()) {
    // Recreate the channel if it has been lost.
    if (gpu_channel_->state() == GpuChannelHost::kLost)
      gpu_channel_ = NULL;
    else
      return gpu_channel_.get();
  }
  // Ensure initialization on the main thread.
  GpuDataManagerImpl::GetInstance();

  EstablishRequest request;
  GetIOLoopProxy()->PostTask(
      FROM_HERE,
      base::Bind(
          &BrowserGpuChannelHostFactory::EstablishGpuChannelOnIO,
          base::Unretained(this),
          &request,
          cause_for_gpu_launch));

  {
    // We're blocking the UI thread, which is generally undesirable.
    // In this case we need to wait for this before we can show any UI /anyway/,
    // so it won't cause additional jank.
    // TODO(piman): Make this asynchronous (http://crbug.com/125248).
    base::ThreadRestrictions::ScopedAllowWait allow_wait;
    request.event.Wait();
  }

  if (request.channel_handle.name.empty())
    return NULL;

  gpu_channel_ = new GpuChannelHost(this, gpu_host_id_, gpu_client_id_);
  gpu_channel_->set_gpu_info(request.gpu_info);
  content::GetContentClient()->SetGpuInfo(request.gpu_info);

  // Connect to the GPU process if a channel name was received.
  gpu_channel_->Connect(request.channel_handle);

  return gpu_channel_.get();
}

}  // namespace content
