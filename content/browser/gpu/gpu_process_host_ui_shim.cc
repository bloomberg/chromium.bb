// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_process_host_ui_shim.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/id_map.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/compositor/gpu_process_transport_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/gpu_host_messages.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/ipc/common/memory_stats.h"
#include "ui/gfx/swap_result.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/java_interfaces.h"
#include "media/mojo/interfaces/android_overlay.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace content {

namespace {

// One of the linux specific headers defines this as a macro.
#ifdef DestroyAll
#undef DestroyAll
#endif

base::LazyInstance<IDMap<GpuProcessHostUIShim*>>::DestructorAtExit
    g_hosts_by_id = LAZY_INSTANCE_INITIALIZER;

#if defined(OS_ANDROID)
template <typename Interface>
void BindJavaInterface(mojo::InterfaceRequest<Interface> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetGlobalJavaInterfaces()->GetInterface(std::move(request));
}

// Binder which posts each request to the UI thread.
template <typename Interface>
void BindJavaInterfaceOnUIThread(mojo::InterfaceRequest<Interface> request) {
  BrowserThread::GetTaskRunnerForThread(BrowserThread::UI)
      ->PostTask(FROM_HERE, base::Bind(&BindJavaInterface<Interface>,
                                       base::Passed(&request)));
}
#endif

}  // namespace

void RouteToGpuProcessHostUIShimTask(int host_id, const IPC::Message& msg) {
  GpuProcessHostUIShim* ui_shim = GpuProcessHostUIShim::FromID(host_id);
  if (ui_shim)
    ui_shim->OnMessageReceived(msg);
}

GpuProcessHostUIShim::GpuProcessHostUIShim(int host_id)
    : host_id_(host_id) {
  g_hosts_by_id.Pointer()->AddWithID(this, host_id_);
#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupportHost()
      ->OnChannelEstablished();
#endif
}

// static
GpuProcessHostUIShim* GpuProcessHostUIShim::Create(int host_id) {
  DCHECK(!FromID(host_id));
  return new GpuProcessHostUIShim(host_id);
}

// static
void GpuProcessHostUIShim::Destroy(int host_id, const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GpuDataManagerImpl::GetInstance()->AddLogMessage(
      logging::LOG_ERROR, "GpuProcessHostUIShim",
      message);

#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupportHost()
      ->OnChannelDestroyed(host_id);
#endif

  delete FromID(host_id);
}

// static
void GpuProcessHostUIShim::DestroyAll() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  while (!g_hosts_by_id.Pointer()->IsEmpty()) {
    IDMap<GpuProcessHostUIShim*>::iterator it(g_hosts_by_id.Pointer());
    delete it.GetCurrentValue();
  }
}

// static
GpuProcessHostUIShim* GpuProcessHostUIShim::FromID(int host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_hosts_by_id.Pointer()->Lookup(host_id);
}

bool GpuProcessHostUIShim::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

#if defined(USE_OZONE)
  if (ui::OzonePlatform::GetInstance()
          ->GetGpuPlatformSupportHost()
          ->OnMessageReceived(message))
    return true;
#endif

  if (message.routing_id() != MSG_ROUTING_CONTROL)
    return false;

  return OnControlMessageReceived(message);
}

GpuProcessHostUIShim::~GpuProcessHostUIShim() {
  DCHECK(CalledOnValidThread());
  g_hosts_by_id.Pointer()->Remove(host_id_);
}

bool GpuProcessHostUIShim::OnControlMessageReceived(
    const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

  IPC_BEGIN_MESSAGE_MAP(GpuProcessHostUIShim, message)
    IPC_MESSAGE_HANDLER(GpuHostMsg_OnLogMessage, OnLogMessage)
    IPC_MESSAGE_HANDLER(GpuHostMsg_GraphicsInfoCollected,
                        OnGraphicsInfoCollected)

    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()

  return true;
}

void GpuProcessHostUIShim::OnLogMessage(
    int level,
    const std::string& header,
    const std::string& message) {
  GpuDataManagerImpl::GetInstance()->AddLogMessage(
      level, header, message);
}

void GpuProcessHostUIShim::OnGraphicsInfoCollected(
    const gpu::GPUInfo& gpu_info) {
  // OnGraphicsInfoCollected is sent back after the GPU process successfully
  // initializes GL.
  TRACE_EVENT0("test_gpu", "OnGraphicsInfoCollected");

  GpuDataManagerImpl::GetInstance()->UpdateGpuInfo(gpu_info);
}

#if defined(OS_ANDROID)
// static
void GpuProcessHostUIShim::RegisterUIThreadMojoInterfaces(
    service_manager::InterfaceRegistry* registry) {
  registry->AddInterface(base::Bind(
      &BindJavaInterfaceOnUIThread<media::mojom::AndroidOverlayProvider>));
}
#endif

}  // namespace content
