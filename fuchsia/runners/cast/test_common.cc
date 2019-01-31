// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/test_common.h"

#include <lib/fidl/cpp/binding.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/service_directory.h"
#include "base/run_loop.h"

zx::channel StartCastComponent(
    const base::StringPiece& cast_url,
    fuchsia::sys::RunnerPtr* sys_runner,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request,
    fidl::Binding<chromium::cast::CastChannel>* cast_channel_binding) {
  // Construct, bind, and populate a ServiceDirectory for publishing
  // the CastChannel service to the CastComponent.
  auto service_list = std::make_unique<fuchsia::sys::ServiceList>();
  zx::channel cast_channel_dir_request, cast_channel_dir_client;
  zx_status_t status = zx::channel::create(0, &cast_channel_dir_request,
                                           &cast_channel_dir_client);
  ZX_CHECK(status == ZX_OK, status) << "zx_channel_create";
  base::fuchsia::ServiceDirectory cast_channel_directory(
      std::move(cast_channel_dir_request));
  base::RunLoop service_connect_runloop;
  cast_channel_directory.AddService(
      chromium::cast::CastChannel::Name_,
      base::BindRepeating(
          [](base::RepeatingClosure on_connect_cb,
             fidl::Binding<chromium::cast::CastChannel>* cast_channel_binding_,
             zx::channel channel) {
            cast_channel_binding_->Bind(std::move(channel));
            on_connect_cb.Run();
          },
          base::Passed(service_connect_runloop.QuitClosure()),
          base::Unretained(cast_channel_binding)));
  service_list->names.push_back(chromium::cast::CastChannel::Name_);
  service_list->host_directory = std::move(cast_channel_dir_client);

  fuchsia::sys::LaunchInfo launch_info;
  launch_info.url = cast_url.as_string();
  launch_info.additional_services = std::move(service_list);

  // Create a channel to pass to the Runner, through which to expose the new
  // component's ServiceDirectory.
  zx::channel service_directory_client;
  status = zx::channel::create(0, &service_directory_client,
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

  // Process the runloop until the CastChannel FIDL service is connected.
  service_connect_runloop.Run();

  // Prepare the service directory for clean teardown.
  cast_channel_directory.RemoveAllServices();

  return service_directory_client;
}
