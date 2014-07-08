// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_helper.h"

#import <Cocoa/Cocoa.h>
#include <IOSurface/IOSurfaceAPI.h>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "content/browser/compositor/browser_compositor_view_mac.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/surface_handle_types_mac.h"
#include "content/common/view_messages.h"

namespace content {
namespace {
typedef std::map<gfx::AcceleratedWidget,std::pair<int,int>> WidgetMap;
base::LazyInstance<WidgetMap> g_widget_map;
base::LazyInstance<base::Lock> g_lock;
}  // namespace

// static
void RenderWidgetHelper::SetRenderWidgetIDForWidget(
    gfx::AcceleratedWidget native_widget,
    int render_process_id,
    int render_widget_id) {
  base::AutoLock lock(g_lock.Get());
  g_widget_map.Get()[native_widget] = std::make_pair(
      render_process_id, render_widget_id);
}

// static
void RenderWidgetHelper::ResetRenderWidgetIDForWidget(
    gfx::AcceleratedWidget native_widget) {
  base::AutoLock lock(g_lock.Get());
  g_widget_map.Get().erase(native_widget);
}

// static
bool RenderWidgetHelper::GetRenderWidgetIDForWidget(
    gfx::AcceleratedWidget native_widget,
    int* render_process_id,
    int* render_widget_id) {
  base::AutoLock lock(g_lock.Get());

  auto found = g_widget_map.Get().find(native_widget);
  if (found != g_widget_map.Get().end()) {
    *render_process_id = found->second.first;
    *render_widget_id = found->second.second;
    return true;
  }

  *render_process_id = 0;
  *render_widget_id = 0;
  return false;
}

// static
void RenderWidgetHelper::OnNativeSurfaceBuffersSwappedOnUIThread(
    const ViewHostMsg_CompositorSurfaceBuffersSwapped_Params& params) {
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
