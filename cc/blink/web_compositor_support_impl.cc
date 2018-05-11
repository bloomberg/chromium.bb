// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_compositor_support_impl.h"

#include <utility>

#include "cc/blink/web_layer_impl.h"
#include "cc/layers/layer.h"

using blink::WebLayer;

namespace cc_blink {

WebCompositorSupportImpl::WebCompositorSupportImpl() = default;

WebCompositorSupportImpl::~WebCompositorSupportImpl() = default;

std::unique_ptr<WebLayer> WebCompositorSupportImpl::CreateLayerFromCCLayer(
    cc::Layer* layer) {
  return std::make_unique<WebLayerImpl>(layer);
}

}  // namespace cc_blink
