// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/resource_provider/resource_provider_impl.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "components/resource_provider/file_utils.h"
#include "mojo/platform_handle/platform_handle_functions.h"
#include "url/gurl.h"

using mojo::ScopedHandle;

namespace resource_provider {
namespace {

ScopedHandle GetHandleForPath(const base::FilePath& path) {
  if (path.empty())
    return ScopedHandle();

  ScopedHandle to_pass;
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    LOG(WARNING) << "file not valid, path=" << path.value();
    return ScopedHandle();
  }

  MojoHandle mojo_handle;
  MojoResult create_result =
      MojoCreatePlatformHandleWrapper(file.TakePlatformFile(), &mojo_handle);
  if (create_result != MOJO_RESULT_OK) {
    LOG(WARNING) << "unable to create wrapper, path=" << path.value()
                 << "result=" << create_result;
    return ScopedHandle();
  }

  return ScopedHandle(mojo::Handle(mojo_handle)).Pass();
}

}  // namespace

ResourceProviderImpl::ResourceProviderImpl(
    const base::FilePath& application_path,
    const std::string& resource_provider_app_url)
    : application_path_(application_path),
      resource_provider_app_url_(resource_provider_app_url) {
  CHECK(!application_path_.empty());
}

ResourceProviderImpl::~ResourceProviderImpl() {
}

void ResourceProviderImpl::GetResources(mojo::Array<mojo::String> paths,
                                        const GetResourcesCallback& callback) {
  mojo::Array<mojo::ScopedHandle> handles;
  if (!paths.is_null()) {
    handles.resize(paths.size());
    for (size_t i = 0; i < paths.size(); ++i) {
      handles[i] = GetHandleForPath(
          GetPathForResourceNamed(application_path_, paths[i]));
    }
  }
  callback.Run(handles.Pass());
}

}  // namespace resource_provider
