// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/console_service_provider.h"

#include "ash/shell.h"
#include "base/bind.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/chromeos/display_configurator.h"

namespace chromeos {

ConsoleServiceProvider::ConsoleServiceProvider() : weak_ptr_factory_(this) {
}

ConsoleServiceProvider::~ConsoleServiceProvider() {
}

void ConsoleServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      kLibCrosServiceInterface, kTakeDisplayOwnership,
      base::Bind(&ConsoleServiceProvider::TakeDisplayOwnership,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ConsoleServiceProvider::OnExported,
                 weak_ptr_factory_.GetWeakPtr()));

  exported_object->ExportMethod(
      kLibCrosServiceInterface, kReleaseDisplayOwnership,
      base::Bind(&ConsoleServiceProvider::ReleaseDisplayOwnership,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ConsoleServiceProvider::OnExported,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ConsoleServiceProvider::TakeDisplayOwnership(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  ash::Shell::GetInstance()->display_configurator()->TakeControl();
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

void ConsoleServiceProvider::ReleaseDisplayOwnership(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  ash::Shell::GetInstance()->display_configurator()->RelinquishControl();
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

void ConsoleServiceProvider::OnExported(const std::string& interface_name,
                                        const std::string& method_name,
                                        bool success) {
  if (!success)
    LOG(ERROR) << "failed to export " << interface_name << "." << method_name;
}

}  // namespace chromeos
