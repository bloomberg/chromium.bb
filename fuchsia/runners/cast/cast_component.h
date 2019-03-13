// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_CAST_COMPONENT_H_
#define FUCHSIA_RUNNERS_CAST_CAST_COMPONENT_H_

#include <fuchsia/modular/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <memory>

#include "base/fuchsia/service_directory.h"
#include "fuchsia/base/agent_manager.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"
#include "fuchsia/runners/cast/cast_channel_bindings.h"
#include "fuchsia/runners/cast/named_message_port_connector.h"
#include "fuchsia/runners/cast/queryable_data_bindings.h"
#include "fuchsia/runners/common/web_component.h"

class CastRunner;

// A specialization of WebComponent which adds Cast-specific services.
class CastComponent : public WebComponent,
                      public chromium::web::NavigationEventObserver {
 public:
  CastComponent(CastRunner* runner,
                std::unique_ptr<base::fuchsia::StartupContext> startup_context,
                fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                    controller_request,
                std::unique_ptr<cr_fuchsia::AgentManager> agent_manager);
  ~CastComponent() override;

 private:
  // WebComponent overrides.
  void DestroyComponent(int termination_exit_code,
                        fuchsia::sys::TerminationReason reason) override;

  // chromium::web::NavigationEventObserver implementation.
  // Triggers the injection of API channels into the page content.
  void OnNavigationStateChanged(
      chromium::web::NavigationEvent change,
      OnNavigationStateChangedCallback callback) override;

  std::unique_ptr<cr_fuchsia::AgentManager> agent_manager_;

  bool constructor_active_ = false;
  NamedMessagePortConnector connector_;
  std::unique_ptr<CastChannelBindings> cast_channel_;
  QueryableDataBindings queryable_data_;

  fuchsia::sys::ServiceProviderPtr agent_services_;
  fuchsia::modular::AgentControllerPtr agent_controller_;

  fidl::Binding<chromium::web::NavigationEventObserver>
      navigation_observer_binding_;

  DISALLOW_COPY_AND_ASSIGN(CastComponent);
};

#endif  // FUCHSIA_RUNNERS_CAST_CAST_COMPONENT_H_
