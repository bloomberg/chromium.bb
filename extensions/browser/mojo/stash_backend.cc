// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mojo/stash_backend.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/environment/async_waiter.h"

namespace extensions {
namespace {

// An implementation of StashService that forwards calls to a StashBackend.
class StashServiceImpl : public StashService {
 public:
  StashServiceImpl(mojo::InterfaceRequest<StashService> request,
                   base::WeakPtr<StashBackend> backend);
  ~StashServiceImpl() override;

  // StashService overrides.
  void AddToStash(mojo::Array<StashedObjectPtr> stash) override;
  void RetrieveStash(
      const mojo::Callback<void(mojo::Array<StashedObjectPtr> stash)>& callback)
      override;

 private:
  mojo::StrongBinding<StashService> binding_;
  base::WeakPtr<StashBackend> backend_;

  DISALLOW_COPY_AND_ASSIGN(StashServiceImpl);
};

StashServiceImpl::StashServiceImpl(mojo::InterfaceRequest<StashService> request,
                                   base::WeakPtr<StashBackend> backend)
    : binding_(this, std::move(request)), backend_(backend) {}

StashServiceImpl::~StashServiceImpl() {
}

void StashServiceImpl::AddToStash(
    mojo::Array<StashedObjectPtr> stashed_objects) {
  if (!backend_)
    return;
  backend_->AddToStash(std::move(stashed_objects));
}

void StashServiceImpl::RetrieveStash(
    const mojo::Callback<void(mojo::Array<StashedObjectPtr>)>& callback) {
  if (!backend_) {
    callback.Run(mojo::Array<StashedObjectPtr>());
    return;
  }
  callback.Run(backend_->RetrieveStash());
}

}  // namespace

// A stash entry for a stashed object. This handles notifications if a handle
// within the stashed object is readable.
class StashBackend::StashEntry {
 public:
  // Construct a StashEntry for |stashed_object|. If |on_handle_readable| is
  // non-null, it will be invoked when any handle on |stashed_object| is
  // readable.
  StashEntry(StashedObjectPtr stashed_object,
             const base::Closure& on_handle_readable);
  ~StashEntry();

  // Returns the stashed object.
  StashedObjectPtr Release();

  // Cancels notifications for handles becoming readable.
  void CancelHandleNotifications();

 private:
  // Invoked when a handle within |stashed_object_| is readable.
  void OnHandleReady(MojoResult result);

  // The waiters that are waiting for handles to be readable.
  std::vector<scoped_ptr<mojo::AsyncWaiter>> waiters_;

  StashedObjectPtr stashed_object_;

  // If non-null, a callback to call when a handle contained within
  // |stashed_object_| is readable.
  const base::Closure on_handle_readable_;
};

StashBackend::StashBackend(const base::Closure& on_handle_readable)
    : on_handle_readable_(on_handle_readable),
      has_notified_(false),
      weak_factory_(this) {
}

StashBackend::~StashBackend() {
}

void StashBackend::AddToStash(mojo::Array<StashedObjectPtr> stashed_objects) {
  for (size_t i = 0; i < stashed_objects.size(); i++) {
    stashed_objects_.push_back(make_scoped_ptr(new StashEntry(
        std::move(stashed_objects[i]),
        has_notified_ ? base::Closure()
                      : base::Bind(&StashBackend::OnHandleReady,
                                   weak_factory_.GetWeakPtr()))));
  }
}

mojo::Array<StashedObjectPtr> StashBackend::RetrieveStash() {
  has_notified_ = false;
  mojo::Array<StashedObjectPtr> result;
  for (auto& entry : stashed_objects_) {
    result.push_back(entry->Release());
  }
  stashed_objects_.clear();
  return result;
}

void StashBackend::BindToRequest(mojo::InterfaceRequest<StashService> request) {
  new StashServiceImpl(std::move(request), weak_factory_.GetWeakPtr());
}

void StashBackend::OnHandleReady() {
  DCHECK(!has_notified_);
  has_notified_ = true;
  for (auto& entry : stashed_objects_) {
    entry->CancelHandleNotifications();
  }
  if (!on_handle_readable_.is_null())
    on_handle_readable_.Run();
}

StashBackend::StashEntry::StashEntry(StashedObjectPtr stashed_object,
                                     const base::Closure& on_handle_readable)
    : stashed_object_(std::move(stashed_object)),
      on_handle_readable_(on_handle_readable) {
  if (on_handle_readable_.is_null() || !stashed_object_->monitor_handles)
    return;

  for (size_t i = 0; i < stashed_object_->stashed_handles.size(); i++) {
    waiters_.push_back(make_scoped_ptr(new mojo::AsyncWaiter(
        stashed_object_->stashed_handles[i].get(), MOJO_HANDLE_SIGNAL_READABLE,
        base::Bind(&StashBackend::StashEntry::OnHandleReady,
                   base::Unretained(this)))));
  }
}

StashBackend::StashEntry::~StashEntry() {
}

StashedObjectPtr StashBackend::StashEntry::Release() {
  waiters_.clear();
  return std::move(stashed_object_);
}

void StashBackend::StashEntry::CancelHandleNotifications() {
  waiters_.clear();
}

void StashBackend::StashEntry::OnHandleReady(MojoResult result) {
  if (result != MOJO_RESULT_OK)
    return;
  on_handle_readable_.Run();
}

}  // namespace extensions
