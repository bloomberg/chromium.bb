// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/test_common.h"

#include "base/fuchsia/fuchsia_logging.h"

zx::channel StartCastComponent(
    const base::StringPiece& cast_url,
    fuchsia::sys::RunnerPtr* sys_runner,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request) {
  fuchsia::sys::LaunchInfo launch_info;
  launch_info.url = cast_url.as_string();

  // Create a channel to pass to the Runner, through which to expose the new
  // component's ServiceDirectory.
  zx::channel service_directory_client;
  zx_status_t status = zx::channel::create(0, &service_directory_client,
                                           &launch_info.directory_request);
  ZX_CHECK(status == ZX_OK, status) << "zx_channel_create";

  fuchsia::sys::StartupInfo startup_info;
  startup_info.launch_info = std::move(launch_info);

  // The FlatNamespace vectors must be non-null, but may be empty.
  startup_info.flat_namespace.paths.resize(0);
  startup_info.flat_namespace.directories.resize(0);

  fuchsia::sys::Package package;
  package.resolved_url = cast_url.as_string();

  sys_runner->get()->StartComponent(std::move(package), std::move(startup_info),
                                    std::move(component_controller_request));
  return service_directory_client;
}
