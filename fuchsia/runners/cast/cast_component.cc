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
    fuchsia::sys::StartupInfo startup_info,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request)
    : WebComponent(runner,
                   std::move(startup_info),
                   std::move(controller_request)),
      navigation_observer_binding_(this) {
  base::AutoReset<bool> constructor_active_reset(&constructor_active_, true);

  if (!additional_services() || std::find(additional_service_names().begin(),
                                          additional_service_names().end(),
                                          chromium::cast::CastChannel::Name_) ==
                                    additional_service_names().end()) {
    LOG(ERROR) << "Component instantiated without required service: "
               << chromium::cast::CastChannel::Name_;
    DestroyComponent(1, fuchsia::sys::TerminationReason::UNSUPPORTED);
    return;
  }

  cast_channel_ = std::make_unique<CastChannelBindings>(
      frame(), &connector_,
      additional_services()->ConnectToService<chromium::cast::CastChannel>(),
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
