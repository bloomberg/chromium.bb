// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/synchronous_compositor_base.h"

#include "base/command_line.h"
#include "base/supports_user_data.h"
#include "content/browser/android/in_process/synchronous_compositor_impl.h"
#include "content/browser/android/synchronous_compositor_host.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/web_contents/web_contents_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/gpu/in_process_gpu_thread.h"
#include "content/public/common/content_switches.h"

namespace content {

class SynchronousCompositorClient;

namespace {

gpu::SyncPointManager* g_sync_point_manager = nullptr;

base::Thread* CreateInProcessGpuThreadForSynchronousCompositor(
    const InProcessChildThreadParams& params) {
  DCHECK(g_sync_point_manager);
  return new InProcessGpuThread(params, g_sync_point_manager);
}

}  // namespace

void SynchronousCompositor::SetGpuService(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service) {
  DCHECK(!g_sync_point_manager);
  g_sync_point_manager = service->sync_point_manager();
  GpuProcessHost::RegisterGpuMainThreadFactory(
      CreateInProcessGpuThreadForSynchronousCompositor);

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kIPCSyncCompositing)) {
    SynchronousCompositorImpl::SetGpuServiceInProc(service);
  }
}

// static
void SynchronousCompositor::SetClientForWebContents(
    WebContents* contents,
    SynchronousCompositorClient* client) {
  DCHECK(contents);
  DCHECK(client);
  WebContentsAndroid* web_contents_android =
      static_cast<WebContentsImpl*>(contents)->GetWebContentsAndroid();
  DCHECK(!web_contents_android->synchronous_compositor_client());
  web_contents_android->set_synchronous_compositor_client(client);
}

// static
scoped_ptr<SynchronousCompositorBase> SynchronousCompositorBase::Create(
    RenderWidgetHostViewAndroid* rwhva,
    WebContents* web_contents) {
  DCHECK(web_contents);
  WebContentsAndroid* web_contents_android =
      static_cast<WebContentsImpl*>(web_contents)->GetWebContentsAndroid();
  if (!web_contents_android->synchronous_compositor_client())
    return nullptr;  // Not using sync compositing.

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kIPCSyncCompositing)) {
    return make_scoped_ptr(new SynchronousCompositorHost(
        rwhva, web_contents_android->synchronous_compositor_client()));
  }
  return make_scoped_ptr(new SynchronousCompositorImpl(
      rwhva, web_contents_android->synchronous_compositor_client()));
}

}  // namespace content
