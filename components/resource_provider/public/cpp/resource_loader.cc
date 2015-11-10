// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/resource_provider/public/cpp/resource_loader.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/platform_handle/platform_handle_functions.h"

namespace resource_provider {
namespace {
base::File GetFileFromHandle(mojo::ScopedHandle handle) {
  CHECK(handle.is_valid());
  MojoPlatformHandle platform_handle;
  CHECK(MojoExtractPlatformHandle(handle.release().value(),
                                  &platform_handle) == MOJO_RESULT_OK);
  return base::File(platform_handle).Pass();
}
}

ResourceLoader::ResourceLoader(mojo::ApplicationImpl* app,
                               const std::set<std::string>& paths)
    : loaded_(false), did_block_(false) {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:resource_provider");
  app->ConnectToService(request.Pass(), &resource_provider_);
  std::vector<std::string> paths_vector(paths.begin(), paths.end());
  resource_provider_->GetResources(
      mojo::Array<mojo::String>::From(paths_vector),
      base::Bind(&ResourceLoader::OnGotResources, base::Unretained(this),
                 paths_vector));
}

ResourceLoader::~ResourceLoader() {
}

bool ResourceLoader::BlockUntilLoaded() {
  if (did_block_ || loaded_)
    return loaded_;

  did_block_ = true;
  resource_provider_.WaitForIncomingResponse();
  return loaded_;
}

base::File ResourceLoader::ReleaseFile(const std::string& path) {
  CHECK(resource_map_.count(path));
  scoped_ptr<base::File> file_wrapper(resource_map_[path].Pass());
  resource_map_.erase(path);
  return file_wrapper->Pass();
}

void ResourceLoader::OnGotResources(const std::vector<std::string>& paths,
                                    mojo::Array<mojo::ScopedHandle> resources) {

  CHECK(!resources.is_null());
  CHECK_EQ(resources.size(), paths.size());
  for (size_t i = 0; i < resources.size(); ++i) {
    resource_map_[paths[i]].reset(
        new base::File(GetFileFromHandle(resources[i].Pass())));
  }
  loaded_ = true;
}

}  // namespace resource_provider
