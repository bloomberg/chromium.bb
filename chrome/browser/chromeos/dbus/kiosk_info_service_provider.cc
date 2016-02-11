// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/kiosk_info_service_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

KioskInfoService::KioskInfoService() : weak_ptr_factory_(this) {}

KioskInfoService::~KioskInfoService() {}

void KioskInfoService::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      kLibCrosServiceInterface, kGetKioskAppRequiredPlatforVersion,
      base::Bind(&KioskInfoService::GetKioskAppRequiredPlatformVersion,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&KioskInfoService::OnExported,
                 weak_ptr_factory_.GetWeakPtr()));
}

void KioskInfoService::OnExported(const std::string& interface_name,
                                  const std::string& method_name,
                                  bool success) {
  if (!success)
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
}

void KioskInfoService::GetKioskAppRequiredPlatformVersion(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  scoped_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendString(
      KioskAppManager::Get()->GetAutoLaunchAppRequiredPlatformVersion());
  response_sender.Run(std::move(response));
}

}  // namespace chromeos
