// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/delegated_frame_resource_collection.h"

#include "base/bind.h"
#include "cc/trees/blocking_task_runner.h"

namespace cc {

DelegatedFrameResourceCollection::DelegatedFrameResourceCollection()
    : client_(NULL), main_thread_runner_(BlockingTaskRunner::current()) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
}

DelegatedFrameResourceCollection::~DelegatedFrameResourceCollection() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
}

void DelegatedFrameResourceCollection::SetClient(
    DelegatedFrameResourceCollectionClient* client) {
  client_ = client;
}

void DelegatedFrameResourceCollection::TakeUnusedResourcesForChildCompositor(
    ReturnedResourceArray* array) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(array->empty());
  array->swap(returned_resources_for_child_compositor_);
}

void DelegatedFrameResourceCollection::ReceivedResources(
    const TransferableResourceArray& resources) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  for (size_t i = 0; i < resources.size(); ++i)
    resource_id_ref_count_map_[resources[i].id].refs_to_return++;
}

void DelegatedFrameResourceCollection::UnrefResources(
    const ReturnedResourceArray& returned) {
  DCHECK(main_thread_checker_.CalledOnValidThread());

  ReturnedResourceArray to_return;

  for (size_t i = 0; i < returned.size(); ++i) {
    ResourceIdRefCountMap::iterator it =
        resource_id_ref_count_map_.find(returned[i].id);
    DCHECK(it != resource_id_ref_count_map_.end());
    DCHECK_GE(it->second.refs_to_wait_for, returned[i].count);
    it->second.refs_to_wait_for -= returned[i].count;
    if (it->second.refs_to_wait_for == 0) {
      to_return.push_back(returned[i]);
      to_return.back().count = it->second.refs_to_return;
      resource_id_ref_count_map_.erase(it);
    }
  }

  if (to_return.empty())
    return;

  returned_resources_for_child_compositor_.insert(
      returned_resources_for_child_compositor_.end(),
      to_return.begin(),
      to_return.end());
  if (client_)
    client_->UnusedResourcesAreAvailable();
}

void DelegatedFrameResourceCollection::RefResources(
    const TransferableResourceArray& resources) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  for (size_t i = 0; i < resources.size(); ++i)
    resource_id_ref_count_map_[resources[i].id].refs_to_wait_for++;
}

ReturnCallback
DelegatedFrameResourceCollection::GetReturnResourcesCallbackForImplThread() {
  return base::Bind(
      &DelegatedFrameResourceCollection::UnrefResourcesOnImplThread,
      this,
      main_thread_runner_);
}

void DelegatedFrameResourceCollection::UnrefResourcesOnImplThread(
    scoped_refptr<BlockingTaskRunner> main_thread_runner,
    const ReturnedResourceArray& returned) {
  main_thread_runner->PostTask(
      FROM_HERE,
      base::Bind(
          &DelegatedFrameResourceCollection::UnrefResources, this, returned));
}

}  // namespace cc
