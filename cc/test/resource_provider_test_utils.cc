// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/resource_provider_test_utils.h"

#include "base/bind.h"

namespace cc {

const ResourceProvider::ResourceIdMap& SendResourceAndGetChildToParentMap(
    const ResourceProvider::ResourceIdArray& resource_ids,
    DisplayResourceProvider* resource_provider,
    LayerTreeResourceProvider* child_resource_provider,
    viz::ContextProvider* child_context_provider) {
  DCHECK(resource_provider);
  DCHECK(child_resource_provider);
  // Transfer resources to the parent.
  std::vector<viz::TransferableResource> send_to_parent;
  int child_id = resource_provider->CreateChild(
      base::BindRepeating([](const std::vector<viz::ReturnedResource>&) {}));
  child_resource_provider->PrepareSendToParent(resource_ids, &send_to_parent,
                                               child_context_provider);
  resource_provider->ReceiveFromChild(child_id, send_to_parent);

  // Return the child to parent map.
  return resource_provider->GetChildToParentMap(child_id);
}

}  // namespace cc
