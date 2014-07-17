// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_helper.h"

#import <Cocoa/Cocoa.h>
#include <IOSurface/IOSurfaceAPI.h>

#include "base/bind.h"
#include "content/browser/compositor/browser_compositor_view_mac.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/surface_handle_types_mac.h"

namespace content {

// static
void RenderWidgetHelper::OnNativeSurfaceBuffersSwappedOnUIThread(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  gfx::AcceleratedWidget native_widget =
      content::GpuSurfaceTracker::Get()->AcquireNativeWidget(params.surface_id);
  IOSurfaceID io_surface_id = content::IOSurfaceIDFromSurfaceHandle(
      params.surface_handle);
  [native_widget gotAcceleratedIOSurfaceFrame:io_surface_id
                          withOutputSurfaceID:params.surface_id
                              withLatencyInfo:params.latency_info
                                withPixelSize:params.size
                              withScaleFactor:params.scale_factor];
}

}  // namespace content
