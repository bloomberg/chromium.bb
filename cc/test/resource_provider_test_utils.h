// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_RESOURCE_PROVIDER_TEST_UTILS_H_
#define CC_TEST_RESOURCE_PROVIDER_TEST_UTILS_H_

#include "cc/resources/display_resource_provider.h"
#include "cc/resources/layer_tree_resource_provider.h"

namespace cc {

// Transfer resources to the parent and return the child to parent map.
const ResourceProvider::ResourceIdMap& SendResourceAndGetChildToParentMap(
    const ResourceProvider::ResourceIdArray& resource_ids,
    DisplayResourceProvider* resource_provider,
    LayerTreeResourceProvider* child_resource_provider,
    viz::ContextProvider* child_context_provider);

}  // namespace cc

#endif  // CC_TEST_RESOURCE_PROVIDER_TEST_UTILS_H_
