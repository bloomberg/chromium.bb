// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_component.h"

#include <lib/fidl/cpp/binding.h>
#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/runners/cast/cast_runner.h"
#include "fuchsia/runners/common/web_component.h"

namespace {
constexpr int kBindingsFailureExitCode = 129;
const char kCastAgentPackageUrl[] =
    "fuchsia-pkg://fuchsia.com/cast_agent#meta/cast_agent.cmx";
}  // namespace

CastComponent::CastComponent(
    CastRunner* runner,
    std::unique_ptr<base::fuchsia::StartupContext> context,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request)
    : WebComponent(runner, std::move(context), std::move(controller_request)),
      queryable_data_(frame(),
                      startup_context()
                          ->incoming_services()
                          ->ConnectToService<chromium::cast::QueryableData>()),
      navigation_observer_binding_(this) {
  base::AutoReset<bool> constructor_active_reset(&constructor_active_, true);

  // Fetch the ComponentContext for this instance.
  fuchsia::modular::ComponentContextPtr component_context;
  startup_context()->incoming_services()->ConnectToService(
      component_context.NewRequest());

  // Request to connect to the Cast agent.
  component_context->ConnectToAgent(kCastAgentPackageUrl,
                                    agent_services_.NewRequest(),
                                    agent_controller_.NewRequest());

  // Bind to the CastChannel service provided by the Agent.
  chromium::cast::CastChannelPtr cast_channel_ptr;
  agent_services_->ConnectToService(
      chromium::cast::CastChannel::Name_,
      cast_channel_ptr.NewRequest().TakeChannel());
  cast_channel_ = std::make_unique<CastChannelBindings>(
      frame(), &connector_, std::move(cast_channel_ptr),
      base::BindOnce(&CastComponent::DestroyComponent, base::Unretained(this),
                     kBindingsFailureExitCode,
                     fuchsia::sys::TerminationReason::INTERNAL_ERROR));

  frame()->SetNavigationEventObserver(
      navigation_observer_binding_.NewBinding());
}

CastComponent::~CastComponent() = default;

void CastComponent::DestroyComponent(int termination_exit_code,
                                     fuchsia::sys::TerminationReason reason) {
  DCHECK(!constructor_active_);

  WebComponent::DestroyComponent(termination_exit_code, reason);
}

void CastComponent::OnNavigationStateChanged(
    chromium::web::NavigationEvent change,
    OnNavigationStateChangedCallback callback) {
  if (change.url)
    connector_.NotifyPageLoad(frame());
  callback();
}
