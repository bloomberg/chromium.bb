// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/kiosk_info_service_provider.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/version.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Creates target version prefix from a given platform version. Target version
// prefix needs to end with a "." when the given platform version does not have
// all 3 components.
std::string GetTargetVersionPrefix(const std::string& platform_version) {
  const base::Version version(platform_version);
  if (!version.IsValid() || version.components().size() > 3u)
    return std::string();

  return version.components().size() < 3u ? version.GetString() + "."
                                          : version.GetString();
}

}  // namespace

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
  std::unique_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendString(GetTargetVersionPrefix(
      KioskAppManager::Get()->GetAutoLaunchAppRequiredPlatformVersion()));
  response_sender.Run(std::move(response));
}

}  // namespace chromeos
