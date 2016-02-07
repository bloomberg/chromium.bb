// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RESOURCE_PROVIDER_PUBLIC_CPP_RESOURCE_LOADER_H_
#define COMPONENTS_RESOURCE_PROVIDER_PUBLIC_CPP_RESOURCE_LOADER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/resource_provider/public/interfaces/resource_provider.mojom.h"
#include "mojo/platform_handle/platform_handle.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/system/handle.h"

namespace base {
class File;
}

namespace mojo {
class Shell;
}

namespace resource_provider {

// ResourceLoader handles making a request to ResourceProvider and blocking
// until the resources are available. Expected use is to create a ResourceLoader
// and shortly thereafter call BlockUntilLoaded() to wait until the resources
// have been obtained.
class ResourceLoader {
 public:
  ResourceLoader(mojo::Shell* shell, const std::set<std::string>& paths);
  ~ResourceLoader();

  // Uses WaitForIncomingMessage() to block until the results are available, or
  // an error occurs. Returns true if the resources were obtained, false on
  // error. This immediately returns if the resources have already been
  // obtained.
  bool BlockUntilLoaded();

  // Releases and returns the file wrapping the handle.
  base::File ReleaseFile(const std::string& path);

  bool loaded() const { return loaded_; }

 private:
  using ResourceMap = std::map<std::string, scoped_ptr<base::File>>;

  // Callback when resources have loaded.
  void OnGotResources(const std::vector<std::string>& paths,
                      mojo::Array<mojo::ScopedHandle> resources);

  ResourceProviderPtr resource_provider_;

  ResourceMap resource_map_;

  bool loaded_;
  bool did_block_;

  DISALLOW_COPY_AND_ASSIGN(ResourceLoader);
};

}  // namespace resource_provider

#endif  // COMPONENTS_RESOURCE_PROVIDER_PUBLIC_CPP_RESOURCE_LOADER_H_
