// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_resize_helper_mac.h"

#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"

namespace content {
namespace {

void HandleGpuIPC(int gpu_host_id, const IPC::Message& message) {
  GpuProcessHostUIShim* host = GpuProcessHostUIShim::FromID(gpu_host_id);
  if (host)
    host->OnMessageReceived(message);
}

void HandleRendererIPC(int render_process_id, const IPC::Message& message) {
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
  if (host)
    host->OnMessageReceived(message);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetResizeHelper

// static
void RenderWidgetResizeHelper::PostRendererProcessMsg(int render_process_id,
                                                      const IPC::Message& msg) {
  ui::WindowResizeHelperMac::Get()->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(HandleRendererIPC, render_process_id, msg),
      base::TimeDelta());
}

// static
void RenderWidgetResizeHelper::PostGpuProcessMsg(int gpu_host_id,
                                                 const IPC::Message& msg) {
  ui::WindowResizeHelperMac::Get()->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(HandleGpuIPC, gpu_host_id, msg), base::TimeDelta());
}

}  // namespace content
