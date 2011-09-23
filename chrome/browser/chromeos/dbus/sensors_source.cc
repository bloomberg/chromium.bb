// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/sensors_source.h"

#include "base/bind.h"
#include "base/callback.h"
#include "content/browser/browser_thread.h"
#include "content/browser/sensors/sensors_provider.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

// TODO(cwolfe): Fix the DEPs so that these can be pulled in from
//               "chromeos/dbus/service_constants.h".
namespace chromeos {
// Sensors service identifiers.
const char kSensorsServiceName[] = "org.chromium.Sensors";
const char kSensorsServicePath[] = "/org/chromium/Sensors";
const char kSensorsServiceInterface[] = "org.chromium.Sensors";
// Sensors signal names.
const char kScreenOrientationChanged[] = "ScreenOrientationChanged";

SensorsSource::SensorsSource() : sensors_proxy_(NULL) {
}

void SensorsSource::Init(dbus::Bus* bus) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  sensors_proxy_ = bus->GetObjectProxy(chromeos::kSensorsServiceName,
                                       chromeos::kSensorsServicePath);
  sensors_proxy_->ConnectToSignal(chromeos::kSensorsServiceInterface,
                                  chromeos::kScreenOrientationChanged,
      base::Bind(&SensorsSource::OrientationChangedReceived, this),
      base::Bind(&SensorsSource::OrientationChangedConnected, this));
}

SensorsSource::~SensorsSource() {
}

void SensorsSource::OrientationChangedReceived(dbus::Signal* signal) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  sensors::ScreenOrientation orientation;

  dbus::MessageReader reader(signal);
  int32 upward = 0;
  if (!reader.PopInt32(&upward)) {
    LOG(WARNING) << "Orientation changed signal had incorrect parameters: "
                 << signal->ToString();
    return;
  }
  VLOG(1) << "Orientation changed to upward " << upward;
  orientation.upward = static_cast<sensors::ScreenOrientation::Side>(upward);

  sensors::Provider::GetInstance()->ScreenOrientationChanged(orientation);
}

void SensorsSource::OrientationChangedConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!success)
    LOG(WARNING) << "Failed to connect to orientation changed signal.";
}

}  // namespace chromeos
