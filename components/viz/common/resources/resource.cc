// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/resource.h"

#include "build/build_config.h"
#include "components/viz/common/quads/shared_bitmap.h"

namespace viz {
namespace internal {

Resource::Resource(int child_id, const TransferableResource& transferable)
    : child_id(child_id),
      transferable(transferable),
      filter(transferable.filter) {
  if (is_gpu_resource_type())
    UpdateSyncToken(transferable.mailbox_holder.sync_token);
  else
    SetSynchronized();
}

Resource::Resource(Resource&& other) = default;

Resource::~Resource() = default;

void Resource::SetLocallyUsed() {
  synchronization_state_ = LOCALLY_USED;
  sync_token_.Clear();
}

void Resource::SetSynchronized() {
  synchronization_state_ = SYNCHRONIZED;
}

void Resource::UpdateSyncToken(const gpu::SyncToken& sync_token) {
  DCHECK(is_gpu_resource_type());
  // An empty sync token may be used if commands are guaranteed to have run on
  // the gpu process or in case of context loss.
  sync_token_ = sync_token;
  synchronization_state_ = sync_token.HasData() ? NEEDS_WAIT : SYNCHRONIZED;
}

int8_t* Resource::GetSyncTokenData() {
  return sync_token_.GetData();
}

bool Resource::ShouldWaitSyncToken() const {
  return synchronization_state_ == NEEDS_WAIT;
}

}  // namespace internal
}  // namespace viz
