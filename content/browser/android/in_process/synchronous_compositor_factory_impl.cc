// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_factory_impl.h"

#include <stdint.h>

#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/android/in_process/synchronous_compositor_impl.h"
#include "content/browser/android/in_process/synchronous_compositor_registry_in_proc.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/android/synchronous_compositor_external_begin_frame_source.h"
#include "content/renderer/android/synchronous_compositor_output_surface.h"
#include "content/renderer/gpu/frame_swap_message_queue.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/ipc/client/gpu_channel_host.h"

namespace content {

SynchronousCompositorFactoryImpl::SynchronousCompositorFactoryImpl() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    // TODO(boliu): Figure out how to deal with this more nicely.
    SynchronousCompositorFactory::SetInstance(this);
  }
}

SynchronousCompositorFactoryImpl::~SynchronousCompositorFactoryImpl() {}

scoped_refptr<base::SingleThreadTaskRunner>
SynchronousCompositorFactoryImpl::GetCompositorTaskRunner() {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
}

std::unique_ptr<cc::OutputSurface>
SynchronousCompositorFactoryImpl::CreateOutputSurface(
    int routing_id,
    uint32_t output_surface_id,
    const scoped_refptr<FrameSwapMessageQueue>& frame_swap_message_queue,
    const scoped_refptr<cc::ContextProvider>& onscreen_context,
    const scoped_refptr<cc::ContextProvider>& worker_context) {
  return base::WrapUnique(new SynchronousCompositorOutputSurface(
      onscreen_context, worker_context, routing_id, output_surface_id,
      SynchronousCompositorRegistryInProc::GetInstance(),
      frame_swap_message_queue));
}

InputHandlerManagerClient*
SynchronousCompositorFactoryImpl::GetInputHandlerManagerClient() {
  return synchronous_input_event_filter();
}

SynchronousInputHandlerProxyClient*
SynchronousCompositorFactoryImpl::GetSynchronousInputHandlerProxyClient() {
  return synchronous_input_event_filter();
}

std::unique_ptr<cc::BeginFrameSource>
SynchronousCompositorFactoryImpl::CreateExternalBeginFrameSource(
    int routing_id) {
  return base::WrapUnique(new SynchronousCompositorExternalBeginFrameSource(
      routing_id, SynchronousCompositorRegistryInProc::GetInstance()));
}

}  // namespace content
