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
}  // namespace

CastComponent::CastComponent(
    CastRunner* runner,
    std::unique_ptr<base::fuchsia::StartupContext> context,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request,
    std::unique_ptr<cr_fuchsia::AgentManager> agent_manager)
    : WebComponent(runner, std::move(context), std::move(controller_request)),
      agent_manager_(std::move(agent_manager)),
      queryable_data_(
          frame(),
          agent_manager_->ConnectToAgentService<chromium::cast::QueryableData>(
              CastRunner::kAgentComponentUrl)),
      navigation_observer_binding_(this) {
  base::AutoReset<bool> constructor_active_reset(&constructor_active_, true);

  // Bind to the CastChannel service provided by the Agent.
  cast_channel_ = std::make_unique<CastChannelBindings>(
      frame(), &connector_,
      agent_manager_->ConnectToAgentService<chromium::cast::CastChannel>(
          CastRunner::kAgentComponentUrl),
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
