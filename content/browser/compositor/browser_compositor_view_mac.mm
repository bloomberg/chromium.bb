// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_view_mac.h"

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/compositor/browser_compositor_ca_layer_tree_mac.h"
#include "content/common/gpu/gpu_messages.h"

////////////////////////////////////////////////////////////////////////////////
// BrowserCompositorViewMac

namespace content {
namespace {

// The number of placeholder objects allocated. If this reaches zero, then
// the BrowserCompositorCALayerTreeMac being held on to for recycling,
// |g_recyclable_ca_layer_tree|, will be freed.
uint32 g_placeholder_count = 0;

// A spare BrowserCompositorCALayerTreeMac kept around for recycling.
base::LazyInstance<scoped_ptr<BrowserCompositorCALayerTreeMac>>
  g_recyclable_ca_layer_tree;

}  // namespace

BrowserCompositorViewMac::BrowserCompositorViewMac(
    BrowserCompositorViewMacClient* client,
    NSView* native_view,
    ui::Layer* ui_root_layer)
      : client_(client),
        native_view_(native_view),
        ui_root_layer_(ui_root_layer) {
  // Try to use the recyclable BrowserCompositorCALayerTreeMac if there is one,
  // otherwise allocate a new one.
  // TODO(ccameron): If there exists a frame in flight (swap has been called
  // by the compositor, but the frame has not arrived from the GPU process
  // yet), then that frame may inappropriately flash in the new view.
  ca_layer_tree_ = g_recyclable_ca_layer_tree.Get().Pass();
  if (!ca_layer_tree_)
    ca_layer_tree_.reset(new BrowserCompositorCALayerTreeMac);
  ca_layer_tree_->SetView(this);
}

BrowserCompositorViewMac::~BrowserCompositorViewMac() {
  // Make this BrowserCompositorCALayerTreeMac recyclable for future instances.
  ca_layer_tree_->ResetView();
  g_recyclable_ca_layer_tree.Get() = ca_layer_tree_.Pass();

  // If there are no placeholders allocated, destroy the recyclable
  // BrowserCompositorCALayerTreeMac that we just populated.
  if (!g_placeholder_count)
    g_recyclable_ca_layer_tree.Get().reset();
}

ui::Compositor* BrowserCompositorViewMac::GetCompositor() const {
  DCHECK(ca_layer_tree_);
  return ca_layer_tree_->compositor();
}

bool BrowserCompositorViewMac::HasFrameOfSize(
    const gfx::Size& dip_size) const {
  if (ca_layer_tree_)
    return ca_layer_tree_->HasFrameOfSize(dip_size);
  return false;
}

void BrowserCompositorViewMac::BeginPumpingFrames() {
  if (ca_layer_tree_)
    ca_layer_tree_->BeginPumpingFrames();
}

void BrowserCompositorViewMac::EndPumpingFrames() {
  if (ca_layer_tree_)
    ca_layer_tree_->EndPumpingFrames();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCompositorViewPlaceholderMac

BrowserCompositorViewPlaceholderMac::BrowserCompositorViewPlaceholderMac() {
  g_placeholder_count += 1;
}

BrowserCompositorViewPlaceholderMac::~BrowserCompositorViewPlaceholderMac() {
  DCHECK_GT(g_placeholder_count, 0u);
  g_placeholder_count -= 1;

  // If there are no placeholders allocated, destroy the recyclable
  // BrowserCompositorCALayerTreeMac.
  if (!g_placeholder_count)
    g_recyclable_ca_layer_tree.Get().reset();
}

}  // namespace content
