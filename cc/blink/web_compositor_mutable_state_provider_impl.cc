// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_compositor_mutable_state_provider_impl.h"

#include "cc/blink/web_compositor_mutable_state_impl.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc_blink {

WebCompositorMutableStateProviderImpl::WebCompositorMutableStateProviderImpl(
    cc::LayerTreeImpl* state,
    cc::LayerTreeMutationMap* mutations)
    : state_(state), mutations_(mutations) {}

WebCompositorMutableStateProviderImpl::
    ~WebCompositorMutableStateProviderImpl() {}

blink::WebPassOwnPtr<blink::WebCompositorMutableState>
WebCompositorMutableStateProviderImpl::getMutableStateFor(uint64_t element_id) {
  cc::LayerTreeImpl::ElementLayers layers =
      state_->GetMutableLayers(element_id);

  if (!layers.main && !layers.scroll)
    return nullptr;

  return blink::adoptWebPtr<blink::WebCompositorMutableState>(
      new WebCompositorMutableStateImpl(&(*mutations_)[element_id], layers.main,
                                        layers.scroll));
}

}  // namespace cc_blink
