// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_coordinator.h"

#include "base/memory/memory_coordinator_client_registry.h"
#include "content/public/common/content_features.h"

namespace content {

// The implementation of MemoryCoordinatorHandle. See memory_coordinator.mojom
// for the role of this class.
class MemoryCoordinatorHandleImpl : public mojom::MemoryCoordinatorHandle {
 public:
  MemoryCoordinatorHandleImpl(mojom::MemoryCoordinatorHandleRequest request)
      : binding_(this, std::move(request)) {
  }

  // mojom::MemoryCoordinatorHandle:
  void AddChild(mojom::ChildMemoryCoordinatorPtr child) override {
    DCHECK(!child_.is_bound());
    child_ = std::move(child);
  }

  mojom::ChildMemoryCoordinatorPtr& child() { return child_; }
  mojo::Binding<mojom::MemoryCoordinatorHandle>& binding() { return binding_; }

 private:
  mojom::ChildMemoryCoordinatorPtr child_;
  mojo::Binding<mojom::MemoryCoordinatorHandle> binding_;

  DISALLOW_COPY_AND_ASSIGN(MemoryCoordinatorHandleImpl);
};

// static
MemoryCoordinator* MemoryCoordinator::GetInstance() {
  if (!base::FeatureList::IsEnabled(features::kMemoryCoordinator))
    return nullptr;
  return base::Singleton<MemoryCoordinator,
                         base::LeakySingletonTraits<MemoryCoordinator>>::get();
}

MemoryCoordinator::MemoryCoordinator() {}

MemoryCoordinator::~MemoryCoordinator() {}

void MemoryCoordinator::CreateHandle(
    int render_process_id,
    mojom::MemoryCoordinatorHandleRequest request) {
  std::unique_ptr<MemoryCoordinatorHandleImpl> handle(
      new MemoryCoordinatorHandleImpl(std::move(request)));
  handle->binding().set_connection_error_handler(
      base::Bind(&MemoryCoordinator::OnConnectionError, base::Unretained(this),
                 render_process_id));
  CreateChildInfoMapEntry(render_process_id, std::move(handle));
}

size_t MemoryCoordinator::NumChildrenForTesting() {
  return children_.size();
}

bool MemoryCoordinator::SetMemoryState(int render_process_id,
                                       mojom::MemoryState memory_state) {
  // Can't set an invalid memory state.
  if (memory_state == mojom::MemoryState::UNKNOWN)
    return false;

  // Can't send a message to a child that doesn't exist.
  auto iter = children_.find(render_process_id);
  if (iter == children_.end())
    return false;

  // A nop doesn't need to be sent, but is considered successful.
  if (iter->second.memory_state == memory_state)
    return true;

  // Update the internal state and send the message.
  iter->second.memory_state = memory_state;
  iter->second.handle->child()->OnStateChange(memory_state);
  return true;
}

mojom::MemoryState MemoryCoordinator::GetMemoryState(
    int render_process_id) const {
  auto iter = children_.find(render_process_id);
  if (iter == children_.end())
    return mojom::MemoryState::UNKNOWN;
  return iter->second.memory_state;
}

void MemoryCoordinator::AddChildForTesting(
    int dummy_render_process_id, mojom::ChildMemoryCoordinatorPtr child) {
  mojom::MemoryCoordinatorHandlePtr mch;
  auto request = mojo::GetProxy(&mch);
  std::unique_ptr<MemoryCoordinatorHandleImpl> handle(
      new MemoryCoordinatorHandleImpl(std::move(request)));
  handle->AddChild(std::move(child));
  CreateChildInfoMapEntry(dummy_render_process_id, std::move(handle));
}

void MemoryCoordinator::OnConnectionError(int render_process_id) {
  children_.erase(render_process_id);
}

void MemoryCoordinator::CreateChildInfoMapEntry(
    int render_process_id,
    std::unique_ptr<MemoryCoordinatorHandleImpl> handle) {
  auto& child_info = children_[render_process_id];
  // Process always start with normal memory state.
  // TODO(chrisha): Consider having memory state be a startup parameter of
  // child processes, allowing them to be launched in a restricted state.
  child_info.memory_state = mojom::MemoryState::NORMAL;
  child_info.handle = std::move(handle);
}

MemoryCoordinator::ChildInfo::ChildInfo() {}

MemoryCoordinator::ChildInfo::ChildInfo(const ChildInfo& rhs) {
  // This is a nop, but exists for compatibility with STL containers.
}

MemoryCoordinator::ChildInfo::~ChildInfo() {}

}  // namespace content
