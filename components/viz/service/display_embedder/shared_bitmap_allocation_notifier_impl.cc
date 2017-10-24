// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/shared_bitmap_allocation_notifier_impl.h"

#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace viz {

SharedBitmapAllocationNotifierImpl::SharedBitmapAllocationNotifierImpl(
    ServerSharedBitmapManager* manager)
    : manager_(manager), binding_(this) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

SharedBitmapAllocationNotifierImpl::~SharedBitmapAllocationNotifierImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ChildDied();
}

void SharedBitmapAllocationNotifierImpl::Bind(
    mojom::SharedBitmapAllocationNotifierRequest request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (binding_.is_bound()) {
    DLOG(ERROR) << "Only one SharedBitmapAllocationNotifierRequest is "
                << "expected from the renderer.";
    return;
  }
  binding_.Bind(std::move(request));
}

void SharedBitmapAllocationNotifierImpl::DidAllocateSharedBitmap(
    mojo::ScopedSharedBufferHandle buffer,
    const SharedBitmapId& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::SharedMemoryHandle memory_handle;
  size_t size;
  MojoResult result = mojo::UnwrapSharedMemoryHandle(
      std::move(buffer), &memory_handle, &size, nullptr);
  DCHECK_EQ(result, MOJO_RESULT_OK);
  this->ChildAllocatedSharedBitmap(size, memory_handle, id);
  last_sequence_number_++;
  for (SharedBitmapAllocationObserver& observer : observers_)
    observer.DidAllocateSharedBitmap(last_sequence_number_);
}

void SharedBitmapAllocationNotifierImpl::DidDeleteSharedBitmap(
    const SharedBitmapId& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  manager_->ChildDeletedSharedBitmap(id);
  owned_bitmaps_.erase(id);
}

void SharedBitmapAllocationNotifierImpl::ChildAllocatedSharedBitmap(
    size_t buffer_size,
    const base::SharedMemoryHandle& handle,
    const SharedBitmapId& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (manager_->ChildAllocatedSharedBitmap(buffer_size, handle, id))
    owned_bitmaps_.insert(id);
}

void SharedBitmapAllocationNotifierImpl::ChildDied() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (const auto& id : owned_bitmaps_)
    manager_->ChildDeletedSharedBitmap(id);
  owned_bitmaps_.clear();
  binding_.Close();
}

void SharedBitmapAllocationNotifierImpl::AddObserver(
    SharedBitmapAllocationObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.AddObserver(observer);
}

void SharedBitmapAllocationNotifierImpl::RemoveObserver(
    SharedBitmapAllocationObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.RemoveObserver(observer);
}

}  // namespace viz
