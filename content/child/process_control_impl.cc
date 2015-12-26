// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/process_control_impl.h"

#include <utility>

#include "base/stl_util.h"
#include "content/public/common/content_client.h"
#include "mojo/shell/static_application_loader.h"
#include "url/gurl.h"

namespace content {

ProcessControlImpl::ProcessControlImpl() {
}

ProcessControlImpl::~ProcessControlImpl() {
  STLDeleteValues(&url_to_loader_map_);
}

void ProcessControlImpl::LoadApplication(
    const mojo::String& url,
    mojo::InterfaceRequest<mojo::Application> request,
    const LoadApplicationCallback& callback) {
  // Only register loaders when we need it.
  if (!has_registered_loaders_) {
    DCHECK(url_to_loader_map_.empty());
    RegisterApplicationLoaders(&url_to_loader_map_);
    has_registered_loaders_ = true;
  }

  GURL application_url = GURL(url.To<std::string>());
  auto it = url_to_loader_map_.find(application_url);
  if (it == url_to_loader_map_.end()) {
    callback.Run(false);
    OnLoadFailed();
    return;
  }

  callback.Run(true);
  it->second->Load(application_url, std::move(request));
}

}  // namespace content
