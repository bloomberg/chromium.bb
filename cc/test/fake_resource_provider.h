// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_RESOURCE_PROVIDER_H_
#define CC_TEST_FAKE_RESOURCE_PROVIDER_H_

#include "cc/resources/display_resource_provider.h"
#include "cc/resources/layer_tree_resource_provider.h"

namespace cc {

class FakeResourceProvider {
 public:
  static std::unique_ptr<LayerTreeResourceProvider>
  CreateLayerTreeResourceProvider(viz::ContextProvider* context_provider) {
    return std::make_unique<LayerTreeResourceProvider>(context_provider, true,
                                                       viz::ResourceSettings());
  }

  static std::unique_ptr<DisplayResourceProvider> CreateDisplayResourceProvider(
      viz::ContextProvider* context_provider,
      viz::SharedBitmapManager* shared_bitmap_manager) {
    return std::make_unique<DisplayResourceProvider>(context_provider,
                                                     shared_bitmap_manager);
  }
};

}  // namespace cc

#endif  // CC_TEST_FAKE_RESOURCE_PROVIDER_H_
