// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/startup_context.h"

#include <fuchsia/io/cpp/fidl.h>

namespace base {
namespace fuchsia {

StartupContext::StartupContext(::fuchsia::sys::StartupInfo startup_info)
    : startup_info_(std::move(startup_info)) {
  // Component manager generates |flat_namespace|, so things are horribly broken
  // if |flat_namespace| is malformed.
  CHECK_EQ(startup_info_.flat_namespace.directories.size(),
           startup_info_.flat_namespace.paths.size());

  // Find the /svc directory and wrap it into a ServiceDirectoryClient.
  for (size_t i = 0; i < startup_info_.flat_namespace.paths.size(); ++i) {
    if (startup_info_.flat_namespace.paths[i] == "/svc") {
      incoming_services_ = std::make_unique<ServiceDirectoryClient>(
          fidl::InterfaceHandle<::fuchsia::io::Directory>(
              std::move(startup_info_.flat_namespace.directories[i])));
      break;
    }
  }
}

StartupContext::~StartupContext() = default;

base::fuchsia::ServiceDirectory* StartupContext::public_services() {
  if (!public_services_ && startup_info_.launch_info.directory_request) {
    public_services_ = std::make_unique<ServiceDirectory>(
        fidl::InterfaceRequest<::fuchsia::io::Directory>(
            std::move(startup_info_.launch_info.directory_request)));
  }
  return public_services_.get();
}

}  // namespace fuchsia
}  // namespace base
