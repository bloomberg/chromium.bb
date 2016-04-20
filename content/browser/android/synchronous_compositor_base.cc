// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/synchronous_compositor_base.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "content/browser/android/synchronous_compositor_host.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/web_contents/web_contents_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/gpu/in_process_gpu_thread.h"
#include "content/public/common/content_switches.h"

namespace content {

class SynchronousCompositorClient;

namespace {

base::LazyInstance<scoped_refptr<gpu::InProcessCommandBuffer::Service>>
    g_gpu_service = LAZY_INSTANCE_INITIALIZER;

base::Thread* CreateInProcessGpuThreadForSynchronousCompositor(
    const InProcessChildThreadParams& params,
    const gpu::GpuPreferences& gpu_preferences) {
  DCHECK(g_gpu_service.Get());
  return new InProcessGpuThread(params, gpu_preferences,
                                g_gpu_service.Get()->sync_point_manager());
}

}  // namespace

void SynchronousCompositor::SetGpuService(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> service) {
  DCHECK(!g_gpu_service.Get());
  g_gpu_service.Get() = service;
  GpuProcessHost::RegisterGpuMainThreadFactory(
      CreateInProcessGpuThreadForSynchronousCompositor);
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
std::unique_ptr<SynchronousCompositorBase> SynchronousCompositorBase::Create(
    RenderWidgetHostViewAndroid* rwhva,
    WebContents* web_contents) {
  DCHECK(web_contents);
  WebContentsAndroid* web_contents_android =
      static_cast<WebContentsImpl*>(web_contents)->GetWebContentsAndroid();
  if (!web_contents_android->synchronous_compositor_client())
    return nullptr;  // Not using sync compositing.

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool async_input =
      !command_line->HasSwitch(switches::kSyncInputForSyncCompositor);
  bool use_in_proc_software_draw =
      command_line->HasSwitch(switches::kSingleProcess);
  return base::WrapUnique(new SynchronousCompositorHost(
      rwhva, web_contents_android->synchronous_compositor_client(), async_input,
      use_in_proc_software_draw));
}

}  // namespace content
