// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_COMPOSITOR_MUTABLE_STATE_PROVIDER_IMPL_H_
#define CC_BLINK_WEB_COMPOSITOR_MUTABLE_STATE_PROVIDER_IMPL_H_

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "cc/animation/layer_tree_mutation.h"
#include "cc/blink/cc_blink_export.h"

#include "third_party/WebKit/public/platform/WebCompositorMutableStateProvider.h"

namespace cc {
class LayerTreeImpl;
}  // namespace cc

namespace cc_blink {

class WebCompositorMutableStateProviderImpl
    : public blink::WebCompositorMutableStateProvider {
 public:
  // TODO(vollick): after slimming paint v2, this will need to operate on
  // property trees, not the layer tree impl.
  //
  // The LayerTreeImpl and the LayerTreeMutationMap are both owned by caller.
  CC_BLINK_EXPORT WebCompositorMutableStateProviderImpl(
      cc::LayerTreeImpl* state,
      cc::LayerTreeMutationMap* mutations);

  CC_BLINK_EXPORT ~WebCompositorMutableStateProviderImpl() override;

  CC_BLINK_EXPORT blink::WebPassOwnPtr<blink::WebCompositorMutableState>
  getMutableStateFor(uint64_t element_id) override WARN_UNUSED_RESULT;

 private:
  cc::LayerTreeImpl* state_;
  cc::LayerTreeMutationMap* mutations_;
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_COMPOSITOR_MUTABLE_STATE_PROVIDER_IMPL_H_
