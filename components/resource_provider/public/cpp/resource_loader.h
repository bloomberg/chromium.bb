// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RESOURCE_PROVIDER_PUBLIC_CPP_RESOURCE_LOADER_H_
#define COMPONENTS_RESOURCE_PROVIDER_PUBLIC_CPP_RESOURCE_LOADER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "components/resource_provider/public/interfaces/resource_provider.mojom.h"
#include "mojo/platform_handle/platform_handle.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "third_party/mojo/src/mojo/public/cpp/system/handle.h"
#include "third_party/mojo/src/mojo/public/interfaces/application/service_provider.mojom.h"

namespace mojo {
class Shell;
}

namespace resource_provider {

// ResourceLoader handles making a request to ResourceProvider and calling a
// callback when the resources are available.
class ResourceLoader {
 public:
  using ResourceMap = std::map<std::string, MojoPlatformHandle>;
  // TODO(sky): convert callback to something that owns handles.
  using LoadedCallback = base::Callback<void(const ResourceMap&)>;

  ResourceLoader(mojo::Shell* shell,
                 const std::set<std::string>& paths,
                 const LoadedCallback& callback);
  ~ResourceLoader();

 private:
  // Callback when resources have loaded.
  void OnGotResources(const std::vector<std::string>& paths,
                      const LoadedCallback& callback,
                      mojo::Array<mojo::ScopedHandle> resources);

  mojo::ServiceProviderPtr resource_provider_service_provider_;

  ResourceProviderPtr resource_provider_;

  DISALLOW_COPY_AND_ASSIGN(ResourceLoader);
};

}  // namespace resource_provider

#endif  // COMPONENTS_RESOURCE_PROVIDER_PUBLIC_CPP_RESOURCE_LOADER_H_
