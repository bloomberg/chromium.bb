// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/screen_lock_service_provider.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

ScreenLockServiceProvider::ScreenLockServiceProvider()
    : weak_ptr_factory_(this) {
}

ScreenLockServiceProvider::~ScreenLockServiceProvider() {}

void ScreenLockServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      kLibCrosServiceInterface,
      kLockScreen,
      base::Bind(&ScreenLockServiceProvider::LockScreen,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ScreenLockServiceProvider::OnExported,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ScreenLockServiceProvider::OnExported(const std::string& interface_name,
                                           const std::string& method_name,
                                           bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "."
               << method_name;
  }
}

void ScreenLockServiceProvider::LockScreen(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  // Please add any additional logic to ScreenLocker::HandleLockScreenRequest()
  // instead of placing it here.
  ScreenLocker::HandleLockScreenRequest();
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

}  // namespace chromeos
