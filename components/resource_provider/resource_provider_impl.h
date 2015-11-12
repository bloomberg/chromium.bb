// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RESOURCE_PROVIDER_RESOURCE_PROVIDER_IMPL_H_
#define COMPONENTS_RESOURCE_PROVIDER_RESOURCE_PROVIDER_IMPL_H_

#include "base/files/file_path.h"
#include "components/resource_provider/public/interfaces/resource_provider.mojom.h"

namespace resource_provider {

// ResourceProvider implementation that loads resources in background threads.
class ResourceProviderImpl : public resource_provider::ResourceProvider {
 public:
  ResourceProviderImpl(const base::FilePath& application_path,
                       const std::string& resource_provider_app_url);
  ~ResourceProviderImpl() override;

 private:
  // ResourceProvider:
  void GetResources(mojo::Array<mojo::String> paths,
                    const GetResourcesCallback& callback) override;

  const base::FilePath application_path_;
  const std::string resource_provider_app_url_;

  DISALLOW_COPY_AND_ASSIGN(ResourceProviderImpl);
};

}  // namespace resource_provider

#endif  // COMPONENTS_RESOURCE_PROVIDER_RESOURCE_PROVIDER_IMPL_H_
