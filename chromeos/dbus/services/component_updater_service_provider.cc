// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/services/component_updater_service_provider.h"

#include <utility>

#include "base/bind.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {
const char kErrorInvalidArgs[] = "org.freedesktop.DBus.Error.InvalidArgs";
const char kErrorInternalError[] = "org.freedesktop.DBus.Error.InternalError";
}  // namespace

ComponentUpdaterServiceProvider::ComponentUpdaterServiceProvider(
    std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)), weak_ptr_factory_(this) {}

ComponentUpdaterServiceProvider::~ComponentUpdaterServiceProvider() {}

void ComponentUpdaterServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      kComponentUpdaterServiceInterface,
      kComponentUpdaterServiceLoadComponentMethod,
      base::Bind(&ComponentUpdaterServiceProvider::LoadComponent,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ComponentUpdaterServiceProvider::OnExported,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ComponentUpdaterServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

void ComponentUpdaterServiceProvider::LoadComponent(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  std::string component_name;
  if (reader.PopString(&component_name)) {
    delegate_->LoadComponent(
        component_name,
        base::Bind(&ComponentUpdaterServiceProvider::OnLoadComponent,
                   weak_ptr_factory_.GetWeakPtr(), method_call,
                   response_sender));
  } else {
    std::unique_ptr<dbus::ErrorResponse> error_response =
        dbus::ErrorResponse::FromMethodCall(
            method_call, kErrorInvalidArgs,
            "Missing component name string argument.");
    response_sender.Run(std::move(error_response));
  }
}

void ComponentUpdaterServiceProvider::OnLoadComponent(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender,
    const std::string& result) {
  if (!result.empty()) {
    std::unique_ptr<dbus::Response> response =
        dbus::Response::FromMethodCall(method_call);
    dbus::MessageWriter writer(response.get());
    writer.AppendString(result);
    response_sender.Run(std::move(response));
  } else {
    std::unique_ptr<dbus::ErrorResponse> error_response =
        dbus::ErrorResponse::FromMethodCall(method_call, kErrorInternalError,
                                            "Failed to load component");
    response_sender.Run(std::move(error_response));
  }
}

}  // namespace chromeos
