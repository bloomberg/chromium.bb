// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/output_surface.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/output_surface_frame.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace cc {

OutputSurface::OutputSurface(scoped_refptr<ContextProvider> context_provider)
    : context_provider_(std::move(context_provider)) {
  DCHECK(context_provider_);
  thread_checker_.DetachFromThread();
}

OutputSurface::OutputSurface(
    std::unique_ptr<SoftwareOutputDevice> software_device)
    : software_device_(std::move(software_device)) {
  DCHECK(software_device_);
  thread_checker_.DetachFromThread();
}

OutputSurface::OutputSurface(
    scoped_refptr<VulkanContextProvider> vulkan_context_provider)
    : vulkan_context_provider_(std::move(vulkan_context_provider)) {
  DCHECK(vulkan_context_provider_);
  thread_checker_.DetachFromThread();
}

OutputSurface::~OutputSurface() {
  // Is destroyed on the thread it is bound to.
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!client_)
    return;

  if (context_provider_) {
    context_provider_->SetLostContextCallback(
        ContextProvider::LostContextCallback());
  }
}

bool OutputSurface::BindToClient(OutputSurfaceClient* client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;

  if (context_provider_) {
    if (!context_provider_->BindToCurrentThread())
      return false;
    context_provider_->SetLostContextCallback(base::Bind(
        &OutputSurface::DidLoseOutputSurface, base::Unretained(this)));
  }
  return true;
}

void OutputSurface::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "OutputSurface::DidLoseOutputSurface");
  client_->DidLoseOutputSurface();
}

}  // namespace cc
