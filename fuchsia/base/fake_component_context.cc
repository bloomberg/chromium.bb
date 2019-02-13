// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/fake_component_context.h"

#include <fuchsia/base/agent_impl.h>
#include <memory>
#include <string>
#include <utility>

namespace cr_fuchsia {

FakeComponentContext::FakeComponentContext(
    AgentImpl::CreateComponentStateCallback create_component_state_callback,
    base::fuchsia::ServiceDirectory* service_directory,
    std::string component_url)
    : agent_impl_(service_directory,
                  std::move(create_component_state_callback)),
      component_url_(component_url) {}

void FakeComponentContext::ConnectToAgent(
    std::string agent_url,
    fidl::InterfaceRequest<::fuchsia::sys::ServiceProvider> services,
    fidl::InterfaceRequest<fuchsia::modular::AgentController> controller) {
  agent_impl_.Connect(component_url_, std::move(services));
}

FakeComponentContext::~FakeComponentContext() = default;

}  // namespace cr_fuchsia
