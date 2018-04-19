// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cec_private/cec_private_api.h"

#include "chromeos/dbus/cec_service_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"

namespace extensions {

namespace {

const char kKioskOnlyError[] =
    "Only kiosk enabled extensions are allowed to use this function.";

}  // namespace

CecPrivateFunction::CecPrivateFunction() = default;

CecPrivateFunction::~CecPrivateFunction() = default;

// Only allow calls from kiosk mode extensions.
bool CecPrivateFunction::PreRunValidation(std::string* error) {
  if (!UIThreadExtensionFunction::PreRunValidation(error))
    return false;

  if (KioskModeInfo::IsKioskEnabled(extension()))
    return true;

  *error = kKioskOnlyError;
  return false;
}

CecPrivateSendStandByFunction::CecPrivateSendStandByFunction() = default;

CecPrivateSendStandByFunction::~CecPrivateSendStandByFunction() = default;

ExtensionFunction::ResponseAction CecPrivateSendStandByFunction::Run() {
  chromeos::DBusThreadManager::Get()->GetCecServiceClient()->SendStandBy();
  return RespondNow(NoArguments());
}

CecPrivateSendWakeUpFunction::CecPrivateSendWakeUpFunction() = default;

CecPrivateSendWakeUpFunction::~CecPrivateSendWakeUpFunction() = default;

ExtensionFunction::ResponseAction CecPrivateSendWakeUpFunction::Run() {
  chromeos::DBusThreadManager::Get()->GetCecServiceClient()->SendWakeUp();
  return RespondNow(NoArguments());
}

}  // namespace extensions
