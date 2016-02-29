// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/process_control_impl.h"

#include <utility>

#include "base/stl_util.h"
#include "content/common/mojo/static_loader.h"
#include "content/public/common/content_client.h"

namespace content {

ProcessControlImpl::ProcessControlImpl() {
}

ProcessControlImpl::~ProcessControlImpl() {
  STLDeleteValues(&name_to_loader_map_);
}

void ProcessControlImpl::LoadApplication(
    const mojo::String& name,
    mojo::InterfaceRequest<mojo::shell::mojom::ShellClient> request,
    const LoadApplicationCallback& callback) {
  // Only register loaders when we need it.
  if (!has_registered_loaders_) {
    DCHECK(name_to_loader_map_.empty());
    RegisterLoaders(&name_to_loader_map_);
    has_registered_loaders_ = true;
  }

  auto it = name_to_loader_map_.find(name);
  if (it == name_to_loader_map_.end()) {
    callback.Run(false);
    OnLoadFailed();
    return;
  }

  callback.Run(true);
  it->second->Load(name, std::move(request));
}

}  // namespace content
