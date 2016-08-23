// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_coordinator/child/child_memory_coordinator_impl.h"

namespace memory_coordinator {

ChildMemoryCoordinatorImpl::ChildMemoryCoordinatorImpl(
    mojom::MemoryCoordinatorHandlePtr parent,
    ChildMemoryCoordinatorDelegate* delegate)
    : binding_(this), parent_(std::move(parent)), delegate_(delegate) {
  DCHECK(delegate_);
  parent_->AddChild(binding_.CreateInterfacePtrAndBind());
}

ChildMemoryCoordinatorImpl::~ChildMemoryCoordinatorImpl() {
}

void ChildMemoryCoordinatorImpl::OnStateChange(mojom::MemoryState state) {
  clients()->Notify(FROM_HERE, &MemoryCoordinatorClient::OnMemoryStateChange,
                    state);
}

#if !defined(OS_ANDROID)
std::unique_ptr<ChildMemoryCoordinatorImpl> CreateChildMemoryCoordinator(
    mojom::MemoryCoordinatorHandlePtr parent,
    ChildMemoryCoordinatorDelegate* delegate) {
  return base::WrapUnique(
      new ChildMemoryCoordinatorImpl(std::move(parent), delegate));
}
#endif

}  // namespace memory_coordinator
