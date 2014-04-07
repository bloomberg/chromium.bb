// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/display_power_service_provider.h"

#include "ash/shell.h"
#include "base/bind.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/chromeos/display_configurator.h"
#include "ui/wm/core/user_activity_detector.h"

namespace chromeos {

DisplayPowerServiceProvider::DisplayPowerServiceProvider()
    : weak_ptr_factory_(this) {
}

DisplayPowerServiceProvider::~DisplayPowerServiceProvider() {}

void DisplayPowerServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      kLibCrosServiceInterface,
      kSetDisplayPower,
      base::Bind(&DisplayPowerServiceProvider::SetDisplayPower,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&DisplayPowerServiceProvider::OnExported,
                 weak_ptr_factory_.GetWeakPtr()));
  exported_object->ExportMethod(
      kLibCrosServiceInterface,
      kSetDisplaySoftwareDimming,
      base::Bind(&DisplayPowerServiceProvider::SetDisplaySoftwareDimming,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&DisplayPowerServiceProvider::OnExported,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DisplayPowerServiceProvider::OnExported(const std::string& interface_name,
                                             const std::string& method_name,
                                             bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "."
               << method_name;
  }
}

void DisplayPowerServiceProvider::SetDisplayPower(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  int int_state = 0;
  if (reader.PopInt32(&int_state)) {
    // Turning displays off when the device becomes idle or on just before
    // we suspend may trigger a mouse move, which would then be incorrectly
    // reported as user activity.  Let the UserActivityDetector
    // know so that it can ignore such events.
    ash::Shell::GetInstance()->user_activity_detector()->
        OnDisplayPowerChanging();

    DisplayPowerState state = static_cast<DisplayPowerState>(int_state);
    ash::Shell::GetInstance()->display_configurator()->SetDisplayPower(
        state, ui::DisplayConfigurator::kSetDisplayPowerNoFlags);
  } else {
    LOG(ERROR) << "Unable to parse " << kSetDisplayPower << " request";
  }

  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

void DisplayPowerServiceProvider::SetDisplaySoftwareDimming(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);
  bool dimmed = false;
  if (reader.PopBool(&dimmed)) {
    ash::Shell::GetInstance()->SetDimming(dimmed);
  } else {
    LOG(ERROR) << "Unable to parse " << kSetDisplaySoftwareDimming
               << " request";
  }
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

}  // namespace chromeos
