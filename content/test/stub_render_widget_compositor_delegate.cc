// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/stub_render_widget_compositor_delegate.h"

#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/swap_promise.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"

namespace content {

void StubRenderWidgetCompositorDelegate::RequestNewLayerTreeFrameSink(
    const LayerTreeFrameSinkCallback& callback) {
  callback.Run(nullptr);
}

bool StubRenderWidgetCompositorDelegate::IsClosing() const {
  return false;
}

std::unique_ptr<cc::SwapPromise>
StubRenderWidgetCompositorDelegate::RequestCopyOfOutputForLayoutTest(
    std::unique_ptr<viz::CopyOutputRequest> request) {
  return nullptr;
}

}  // namespace content
