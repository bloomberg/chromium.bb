// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/fake_component_context.h"

#include <fuchsia/base/agent_impl.h>
#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"

namespace cr_fuchsia {

FakeComponentContext::FakeComponentContext(
    AgentImpl::CreateComponentStateCallback create_component_state_callback,
    base::fuchsia::ServiceDirectory* service_directory,
    base::StringPiece component_url)
    : binding_(service_directory, this),
      // Publishing the Agent to |service_directory| is not necessary, but
      // also shouldn't do any harm.
      agent_impl_(service_directory,
                  std::move(create_component_state_callback)),
      component_url_(component_url.as_string()) {}

void FakeComponentContext::ConnectToAgent(
    std::string agent_url,
    fidl::InterfaceRequest<::fuchsia::sys::ServiceProvider> services,
    fidl::InterfaceRequest<fuchsia::modular::AgentController> controller) {
  agent_impl_.Connect(component_url_, std::move(services));
}

void FakeComponentContext::NotImplemented_(const std::string& name) {
  NOTIMPLEMENTED() << " API: " << name;
}

FakeComponentContext::~FakeComponentContext() = default;

}  // namespace cr_fuchsia
