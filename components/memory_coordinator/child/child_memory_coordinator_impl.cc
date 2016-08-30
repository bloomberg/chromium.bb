// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_coordinator/child/child_memory_coordinator_impl.h"

#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"

namespace memory_coordinator {

namespace {

base::LazyInstance<base::Lock>::Leaky g_lock = LAZY_INSTANCE_INITIALIZER;
ChildMemoryCoordinatorImpl* g_child_memory_coordinator = nullptr;

}  // namespace

// static
ChildMemoryCoordinatorImpl* ChildMemoryCoordinatorImpl::GetInstance() {
  base::AutoLock lock(*g_lock.Pointer());
  return g_child_memory_coordinator;
}

ChildMemoryCoordinatorImpl::ChildMemoryCoordinatorImpl(
    mojom::MemoryCoordinatorHandlePtr parent,
    ChildMemoryCoordinatorDelegate* delegate)
    : binding_(this), parent_(std::move(parent)), delegate_(delegate) {
  base::AutoLock lock(*g_lock.Pointer());
  DCHECK(delegate_);
  DCHECK(!g_child_memory_coordinator);
  g_child_memory_coordinator = this;
  parent_->AddChild(binding_.CreateInterfacePtrAndBind());
}

ChildMemoryCoordinatorImpl::~ChildMemoryCoordinatorImpl() {
  base::AutoLock lock(*g_lock.Pointer());
  DCHECK(g_child_memory_coordinator == this);
  g_child_memory_coordinator = nullptr;
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
