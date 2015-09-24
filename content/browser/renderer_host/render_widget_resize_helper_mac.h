// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_RESIZE_HELPER_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_RESIZE_HELPER_MAC_H_

#include "base/macros.h"

namespace IPC {
class Message;
}

namespace content {

// Provides functions for content to synchronize with window resize tasks being
// managed by ui::WindowResizeHelperMac. WindowResizeHelperMac must be
// initialized before calling these functions.
//
// The IPCs that are required to create new frames for smooth resize are sent
// to the ui::WindowResizeHelperMac singleton using the PostRendererProcessMsg()
// and PostGpuProcessMsg() methods. Usually those IPCs arrive on the IO thread
// and are posted as tasks to the UI thread either by the RenderMessageFilter
// (for renderer processes) or the GpuProcessHostUIShim (for the GPU process).
// By posting them via RenderWidgetResizeHelper instead, it allows a smooth
// window resize to be controlled by WindowResizeHelperMac.
class RenderWidgetResizeHelper {
 public:
  // IO THREAD ONLY -----------------------------------------------------------

  // This will cause |msg| to be handled by the RenderProcessHost corresponding
  // to |render_process_id|, on the UI thread. This will either happen when the
  // ordinary message loop would run it, or potentially earlier in a call to
  // WindowResizeHelperMac::WaitForSingleTaskToRun().
  static void PostRendererProcessMsg(int render_process_id,
                                     const IPC::Message& msg);

  // This is similar to PostRendererProcessMsg, but will handle the message in
  // the GpuProcessHostUIShim corresponding to |gpu_host_id|.
  static void PostGpuProcessMsg(int gpu_host_id, const IPC::Message& msg);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(RenderWidgetResizeHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_RESIZE_HELPER_MAC_H_
