// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/liveness_service_provider.h"

#include "base/bind.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using content::BrowserThread;

namespace chromeos {

LivenessServiceProvider::LivenessServiceProvider() : weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

LivenessServiceProvider::~LivenessServiceProvider() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void LivenessServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  exported_object->ExportMethod(
      kLibCrosServiceInterface,
      kCheckLiveness,
      base::Bind(&LivenessServiceProvider::CheckLiveness,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&LivenessServiceProvider::OnExported,
                 weak_ptr_factory_.GetWeakPtr()));
}

void LivenessServiceProvider::OnExported(const std::string& interface_name,
                                         const std::string& method_name,
                                         bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "."
               << method_name;
  }
}

void LivenessServiceProvider::CheckLiveness(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  dbus::Response* response = dbus::Response::FromMethodCall(method_call);
  response_sender.Run(response);
}

}  // namespace chromeos
