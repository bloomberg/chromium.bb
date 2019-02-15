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

fidl::InterfaceHandle<fuchsia::io::Directory> StartCastComponent(
    const base::StringPiece& cast_url,
    fuchsia::sys::RunnerPtr* sys_runner,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request,
    fidl::Binding<chromium::cast::CastChannel>* cast_channel_binding) {
  // Construct, bind, and populate a ServiceDirectory for publishing
  // the CastChannel service to the CastComponent.
  fidl::InterfaceHandle<fuchsia::io::Directory> directory;
  base::fuchsia::ServiceDirectory cast_channel_directory(
      directory.NewRequest());
  base::RunLoop service_connect_runloop;
  cast_channel_directory.AddService(base::BindRepeating(
      [](base::RepeatingClosure on_connect_cb,
         fidl::Binding<chromium::cast::CastChannel>* cast_channel_binding_,
         fidl::InterfaceRequest<chromium::cast::CastChannel> request) {
        cast_channel_binding_->Bind(std::move(request));
        on_connect_cb.Run();
      },
      base::Passed(service_connect_runloop.QuitClosure()),
      base::Unretained(cast_channel_binding)));

  // Configure the Runner, including a service directory channel to publish
  // services to.
  fuchsia::sys::StartupInfo startup_info;
  startup_info.launch_info.url = cast_url.as_string();

  fidl::InterfaceHandle<fuchsia::io::Directory> component_services;
  startup_info.launch_info.directory_request =
      component_services.NewRequest().TakeChannel();

  // Publish the ServiceDirectory into the FlatNamespace for CastChannel to be
  // picked up from.
  startup_info.flat_namespace.paths.emplace_back("/svc");
  startup_info.flat_namespace.directories.emplace_back(directory.TakeChannel());

  fuchsia::sys::Package package;
  package.resolved_url = cast_url.as_string();

  sys_runner->get()->StartComponent(std::move(package), std::move(startup_info),
                                    std::move(component_controller_request));

  // Process the runloop until the CastChannel FIDL service is connected.
  service_connect_runloop.Run();

  // Prepare the service directory for clean teardown.
  cast_channel_directory.RemoveAllServices();

  return component_services;
}
