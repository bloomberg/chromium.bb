// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aura/software_browser_compositor_output_surface.h"

#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/software_output_device.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "ui/events/latency_info.h"

namespace content {

SoftwareBrowserCompositorOutputSurface::SoftwareBrowserCompositorOutputSurface(
    scoped_ptr<cc::SoftwareOutputDevice> software_device)
    : cc::OutputSurface(software_device.Pass()) {}

void SoftwareBrowserCompositorOutputSurface::SwapBuffers(
    cc::CompositorFrame* frame) {
  ui::LatencyInfo latency_info = frame->metadata.latency_info;
  latency_info.AddLatencyNumber(
      ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0);

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(
          &RenderWidgetHostImpl::CompositorFrameDrawn,
          latency_info));
}

}  // namespace content
