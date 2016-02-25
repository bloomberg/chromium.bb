// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/application_instance.h"

#include <stdint.h>

#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/stl_util.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/shell/application_manager.h"
#include "mojo/shell/capability_filter.h"

namespace mojo {
namespace shell {

ApplicationInstance::ApplicationInstance(
    mojom::ShellClientPtr shell_client,
    ApplicationManager* manager,
    const Identity& identity)
    : manager_(manager),
      id_(GenerateUniqueID()),
      identity_(identity),
      allow_any_application_(identity.filter().size() == 1 &&
                             identity.filter().count("*") == 1),
      shell_client_(std::move(shell_client)),
      pid_receiver_binding_(this),
      native_runner_(nullptr),
      pid_(base::kNullProcessId) {
  DCHECK_NE(kInvalidApplicationID, id_);
}

ApplicationInstance::~ApplicationInstance() {}

void ApplicationInstance::InitializeApplication() {
  shell_client_->Initialize(connectors_.CreateInterfacePtrAndBind(this),
                            identity_.url().spec(), id_, identity_.user_id());
  connectors_.set_connection_error_handler(
      base::Bind(&ApplicationManager::OnApplicationInstanceError,
                 base::Unretained(manager_), base::Unretained(this)));
}

void ApplicationInstance::ConnectToClient(scoped_ptr<ConnectParams> params) {
  params->connect_callback().Run(id_);
  AllowedInterfaces interfaces;
  interfaces.insert("*");
  if (!params->source().is_null())
    interfaces = GetAllowedInterfaces(params->source().filter(), identity_);

  ApplicationInstance* source =
      manager_->GetApplicationInstance(params->source());
  uint32_t source_id = source ? source->id() : kInvalidApplicationID;
  shell_client_->AcceptConnection(
      params->source().url().spec(), params->source().user_id(), source_id,
      params->TakeRemoteInterfaces(), params->TakeLocalInterfaces(),
      Array<String>::From(interfaces), params->target().url().spec());
}

void ApplicationInstance::SetNativeRunner(NativeRunner* native_runner) {
  native_runner_ = native_runner;
}

void ApplicationInstance::BindPIDReceiver(
    InterfaceRequest<mojom::PIDReceiver> pid_receiver) {
  pid_receiver_binding_.Bind(std::move(pid_receiver));
}

// Connector implementation:
void ApplicationInstance::Connect(
    const String& app_url,
    uint32_t user_id,
    shell::mojom::InterfaceProviderRequest remote_interfaces,
    shell::mojom::InterfaceProviderPtr local_interfaces,
    const ConnectCallback& callback) {
  GURL url = app_url.To<GURL>();
  if (!url.is_valid()) {
    LOG(ERROR) << "Error: invalid URL: " << app_url;
    callback.Run(kInvalidApplicationID);
    return;
  }
  if (allow_any_application_ ||
      identity_.filter().find(url.spec()) != identity_.filter().end()) {
    scoped_ptr<ConnectParams> params(new ConnectParams);
    params->set_source(identity_);
    params->set_target(Identity(url, std::string(), user_id));
    params->set_remote_interfaces(std::move(remote_interfaces));
    params->set_local_interfaces(std::move(local_interfaces));
    params->set_connect_callback(callback);
    manager_->Connect(std::move(params));
  } else {
    LOG(WARNING) << "CapabilityFilter prevented connection from: " <<
        identity_.url() << " to: " << url.spec();
    callback.Run(kInvalidApplicationID);
  }
}

void ApplicationInstance::Clone(mojom::ConnectorRequest request) {
  connectors_.AddBinding(this, std::move(request));
}

void ApplicationInstance::SetPID(uint32_t pid) {
  // This will call us back to update pid_.
  manager_->ApplicationPIDAvailable(id_, pid);
}

uint32_t ApplicationInstance::GenerateUniqueID() const {
  static uint32_t id = kInvalidApplicationID;
  ++id;
  CHECK_NE(kInvalidApplicationID, id);
  return id;
}

}  // namespace shell
}  // namespace mojo
