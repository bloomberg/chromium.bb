// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/chrome_features_service_provider.h"

#include <utility>

#include "base/bind.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

ChromeFeaturesServiceProvider::ChromeFeaturesServiceProvider(
    std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)), weak_ptr_factory_(this) {}

ChromeFeaturesServiceProvider::~ChromeFeaturesServiceProvider() = default;

void ChromeFeaturesServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      kChromeFeaturesServiceInterface,
      kChromeFeaturesServiceIsCrostiniEnabledMethod,
      base::BindRepeating(&ChromeFeaturesServiceProvider::IsCrostiniEnabled,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ChromeFeaturesServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      kChromeFeaturesServiceInterface,
      kChromeFeaturesServiceIsUsbguardEnabledMethod,
      base::BindRepeating(&ChromeFeaturesServiceProvider::IsUsbguardEnabled,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ChromeFeaturesServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      kChromeFeaturesServiceInterface,
      kChromeFeaturesServiceIsShillSandboxingEnabledMethod,
      base::BindRepeating(
          &ChromeFeaturesServiceProvider::IsShillSandboxingEnabled,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ChromeFeaturesServiceProvider::OnExported,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ChromeFeaturesServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

void ChromeFeaturesServiceProvider::IsCrostiniEnabled(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  std::string user_id_hash;

  if (!reader.PopString(&user_id_hash)) {
    LOG(ERROR) << "Failed to pop user_id_hash from incoming message.";
    response_sender.Run(dbus::ErrorResponse::FromMethodCall(
        method_call, DBUS_ERROR_INVALID_ARGS, "No user_id_hash string arg"));
    return;
  }

  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(delegate_->IsCrostiniEnabled(user_id_hash));
  response_sender.Run(std::move(response));
}

void ChromeFeaturesServiceProvider::IsUsbguardEnabled(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(delegate_->IsUsbguardEnabled());
  response_sender.Run(std::move(response));
}

void ChromeFeaturesServiceProvider::IsShillSandboxingEnabled(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendBool(delegate_->IsShillSandboxingEnabled());
  response_sender.Run(std::move(response));
}

}  // namespace chromeos
