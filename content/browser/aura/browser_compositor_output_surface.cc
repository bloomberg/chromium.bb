// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aura/browser_compositor_output_surface.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/aura/reflector_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"

namespace content {

bool SizeRequiresSoftwareCompositor(gfx::Size size) {
#if defined(OS_WIN)
  // Some GPU drivers have issues with windows smaller than 64 pixels wide or
  // tall, so switch those to software if necessary. Switch to hardware if the
  // size of the window increases to greater than that. http://crbug.com/286609
  return size.width() != 0 && size.height() != 0 &&
         (size.width() < 64 || size.height() < 64);
#else
  return false;
#endif
}

BrowserCompositorOutputSurface::BrowserCompositorOutputSurface(
    const scoped_refptr<ContextProviderCommandBuffer>& context_provider,
    int surface_id,
    IDMap<BrowserCompositorOutputSurface>* output_surface_map,
    base::MessageLoopProxy* compositor_message_loop,
    base::WeakPtr<ui::Compositor> compositor)
    : OutputSurface(context_provider),
      surface_id_(surface_id),
      output_surface_map_(output_surface_map),
      compositor_message_loop_(compositor_message_loop),
      compositor_(compositor),
      failed_creating_gpu_compositor_(false) {
  Initialize();
}

BrowserCompositorOutputSurface::BrowserCompositorOutputSurface(
    bool failed_creating_gpu_compositor,
    scoped_ptr<cc::SoftwareOutputDevice> software_device,
    int surface_id,
    IDMap<BrowserCompositorOutputSurface>* output_surface_map,
    base::MessageLoopProxy* compositor_message_loop,
    base::WeakPtr<ui::Compositor> compositor)
    : OutputSurface(software_device.Pass()),
      surface_id_(surface_id),
      output_surface_map_(output_surface_map),
      compositor_message_loop_(compositor_message_loop),
      compositor_(compositor),
      failed_creating_gpu_compositor_(failed_creating_gpu_compositor) {
  Initialize();
}

BrowserCompositorOutputSurface::~BrowserCompositorOutputSurface() {
  DCHECK(CalledOnValidThread());
  if (!HasClient())
    return;
  output_surface_map_->Remove(surface_id_);
}

void BrowserCompositorOutputSurface::Initialize() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUIMaxFramesPending)) {
    std::string string_value = command_line->GetSwitchValueASCII(
        switches::kUIMaxFramesPending);
    int int_value;
    if (base::StringToInt(string_value, &int_value))
      capabilities_.max_frames_pending = int_value;
    else
      LOG(ERROR) << "Trouble parsing --" << switches::kUIMaxFramesPending;
  }
  capabilities_.adjust_deadline_for_parent = false;

  DetachFromThread();
}

bool BrowserCompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  DCHECK(CalledOnValidThread());

  if (!OutputSurface::BindToClient(client))
    return false;

  output_surface_map_->AddWithID(this, surface_id_);
  if (reflector_)
    reflector_->OnSourceSurfaceReady(surface_id_);
  return true;
}

void BrowserCompositorOutputSurface::Reshape(gfx::Size size,
                                             float scale_factor) {
  OutputSurface::Reshape(size, scale_factor);
  if (reflector_.get())
    reflector_->OnReshape(size);
  bool use_software = SizeRequiresSoftwareCompositor(size);
  if (software_device() && !use_software && !failed_creating_gpu_compositor_) {
    DidLoseOutputSurface();
  }
  if (!software_device() && use_software)
    DidLoseOutputSurface();
}

void BrowserCompositorOutputSurface::OnUpdateVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  DCHECK(CalledOnValidThread());
  DCHECK(HasClient());
  OnVSyncParametersChanged(timebase, interval);
  compositor_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ui::Compositor::OnUpdateVSyncParameters,
                 compositor_, timebase, interval));
}

void BrowserCompositorOutputSurface::SetReflector(ReflectorImpl* reflector) {
  reflector_ = reflector;
}

}  // namespace content
