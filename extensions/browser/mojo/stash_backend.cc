// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mojo/stash_backend.h"

namespace extensions {

// An implementation of StashService that forwards calls to a StashBackend.
class StashServiceImpl : public mojo::InterfaceImpl<StashService> {
 public:
  explicit StashServiceImpl(base::WeakPtr<StashBackend> backend);
  ~StashServiceImpl() override;

  // mojo::InterfaceImpl<StashService> overrides.
  void AddToStash(mojo::Array<StashedObjectPtr> stash) override;
  void RetrieveStash(
      const mojo::Callback<void(mojo::Array<StashedObjectPtr> stash)>& callback)
      override;

 private:
  base::WeakPtr<StashBackend> backend_;

  DISALLOW_COPY_AND_ASSIGN(StashServiceImpl);
};

StashBackend::StashBackend() : weak_factory_(this) {
}

StashBackend::~StashBackend() {
}

void StashBackend::AddToStash(mojo::Array<StashedObjectPtr> stashed_objects) {
  for (size_t i = 0; i < stashed_objects.size(); i++) {
    stashed_objects_.push_back(stashed_objects[i].Pass());
  }
}

mojo::Array<StashedObjectPtr> StashBackend::RetrieveStash() {
  if (stashed_objects_.is_null())
    stashed_objects_.resize(0);
  return stashed_objects_.Pass();
}

void StashBackend::BindToRequest(mojo::InterfaceRequest<StashService> request) {
  mojo::BindToRequest(new StashServiceImpl(weak_factory_.GetWeakPtr()),
                      &request);
}

StashServiceImpl::StashServiceImpl(base::WeakPtr<StashBackend> backend)
    : backend_(backend) {
}

StashServiceImpl::~StashServiceImpl() {
}

void StashServiceImpl::AddToStash(
    mojo::Array<StashedObjectPtr> stashed_objects) {
  if (!backend_)
    return;
  backend_->AddToStash(stashed_objects.Pass());
}

void StashServiceImpl::RetrieveStash(
    const mojo::Callback<void(mojo::Array<StashedObjectPtr>)>& callback) {
  if (!backend_) {
    callback.Run(mojo::Array<StashedObjectPtr>(0));
    return;
  }
  callback.Run(backend_->RetrieveStash());
}

}  // namespace extensions
