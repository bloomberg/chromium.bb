// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_view_mac.h"

#include <stdint.h>

#include <utility>

#include "base/lazy_instance.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/public/browser/context_factory.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"

////////////////////////////////////////////////////////////////////////////////
// BrowserCompositorMac

namespace content {

namespace {

// Set when no browser compositors should remain alive.
bool g_has_shut_down = false;

// The number of placeholder objects allocated. If this reaches zero, then
// the BrowserCompositorMac being held on to for recycling,
// |g_recyclable_browser_compositor|, will be freed.
uint32_t g_placeholder_count = 0;

// A spare BrowserCompositorMac kept around for recycling.
base::LazyInstance<scoped_ptr<BrowserCompositorMac>>
  g_recyclable_browser_compositor;

bool WidgetNeedsGLFinishWorkaround() {
  return GpuDataManagerImpl::GetInstance()->IsDriverBugWorkaroundActive(
      gpu::FORCE_GL_FINISH_AFTER_COMPOSITING);
}

}  // namespace

BrowserCompositorMac::BrowserCompositorMac()
    : accelerated_widget_mac_(
          new ui::AcceleratedWidgetMac(WidgetNeedsGLFinishWorkaround())),
      compositor_(content::GetContextFactory(),
                  ui::WindowResizeHelperMac::Get()->task_runner()) {
  compositor_.SetAcceleratedWidget(
      accelerated_widget_mac_->accelerated_widget());
  compositor_.SetLocksWillTimeOut(false);
  Suspend();
  compositor_.AddObserver(this);
}

BrowserCompositorMac::~BrowserCompositorMac() {
  compositor_.RemoveObserver(this);
}

void BrowserCompositorMac::Suspend() {
  compositor_suspended_lock_ = compositor_.GetCompositorLock();
}

void BrowserCompositorMac::Unsuspend() {
  compositor_suspended_lock_ = nullptr;
}

void BrowserCompositorMac::OnCompositingDidCommit(
    ui::Compositor* compositor_that_did_commit) {
  DCHECK_EQ(compositor_that_did_commit, compositor());
  content::ImageTransportFactory::GetInstance()
      ->SetCompositorSuspendedForRecycle(compositor(), false);
}

// static
scoped_ptr<BrowserCompositorMac> BrowserCompositorMac::Create() {
  DCHECK(ui::WindowResizeHelperMac::Get()->task_runner());
  if (g_recyclable_browser_compositor.Get())
    return std::move(g_recyclable_browser_compositor.Get());
  return scoped_ptr<BrowserCompositorMac>(new BrowserCompositorMac);
}

// static
void BrowserCompositorMac::Recycle(
    scoped_ptr<BrowserCompositorMac> compositor) {
  DCHECK(compositor);
  content::ImageTransportFactory::GetInstance()
      ->SetCompositorSuspendedForRecycle(compositor->compositor(), true);

  // It is an error to have a browser compositor continue to exist after
  // shutdown.
  CHECK(!g_has_shut_down);

  // Make this BrowserCompositorMac recyclable for future instances.
  g_recyclable_browser_compositor.Get().swap(compositor);

  // If there are no placeholders allocated, destroy the recyclable
  // BrowserCompositorMac that we just populated.
  if (!g_placeholder_count)
    g_recyclable_browser_compositor.Get().reset();
}

// static
void BrowserCompositorMac::DisableRecyclingForShutdown() {
  g_has_shut_down = true;
  g_recyclable_browser_compositor.Get().reset();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCompositorMacPlaceholder

BrowserCompositorMacPlaceholder::BrowserCompositorMacPlaceholder() {
  g_placeholder_count += 1;
}

BrowserCompositorMacPlaceholder::~BrowserCompositorMacPlaceholder() {
  DCHECK_GT(g_placeholder_count, 0u);
  g_placeholder_count -= 1;

  // If there are no placeholders allocated, destroy the recyclable
  // BrowserCompositorMac.
  if (!g_placeholder_count)
    g_recyclable_browser_compositor.Get().reset();
}

}  // namespace content
