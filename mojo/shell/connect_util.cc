// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/connect_util.h"

#include <utility>

#include "mojo/shell/application_manager.h"
#include "mojo/shell/capability_filter.h"
#include "mojo/shell/connect_to_application_params.h"

namespace mojo {
namespace shell {

ScopedMessagePipeHandle ConnectToServiceByName(
    ApplicationManager* application_manager,
    const GURL& application_url,
    const std::string& interface_name) {
  ServiceProviderPtr services;
  scoped_ptr<ConnectToApplicationParams> params(new ConnectToApplicationParams);
  params->SetTarget(Identity(application_url, std::string(),
                             GetPermissiveCapabilityFilter()));
  params->set_services(GetProxy(&services));
  application_manager->ConnectToApplication(std::move(params));
  MessagePipe pipe;
  services->ConnectToService(interface_name, std::move(pipe.handle1));
  return std::move(pipe.handle0);
}

}  // namespace shell
}  // namespace mojo
