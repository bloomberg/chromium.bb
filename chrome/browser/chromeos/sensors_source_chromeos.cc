// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/sensors_source_chromeos.h"

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
}  // namespace chromeos

namespace sensors {

SensorsSourceChromeos::SensorsSourceChromeos() : sensors_proxy_(NULL) {
}

bool SensorsSourceChromeos::Init() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(BrowserThread::IsMessageLoopValid(BrowserThread::FILE));

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  options.connection_type = dbus::Bus::PRIVATE;
  options.dbus_thread_message_loop_proxy =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE);
  bus_ = new dbus::Bus(options);

  sensors_proxy_ = bus_->GetObjectProxy(chromeos::kSensorsServiceName,
                                        chromeos::kSensorsServicePath);
  sensors_proxy_->ConnectToSignal(chromeos::kSensorsServiceInterface,
                                  chromeos::kScreenOrientationChanged,
      base::Bind(&SensorsSourceChromeos::OrientationChangedReceived, this),
      base::Bind(&SensorsSourceChromeos::OrientationChangedConnected, this));
  return true;
}

void SensorsSourceChromeos::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (bus_)
    bus_->ShutdownOnDBusThreadAndBlock();
}

SensorsSourceChromeos::~SensorsSourceChromeos() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Stop();
}

void SensorsSourceChromeos::OrientationChangedReceived(dbus::Signal* signal) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ScreenOrientation orientation;

  dbus::MessageReader reader(signal);
  int32 upward = 0;
  if (!reader.PopInt32(&upward)) {
    LOG(WARNING) << "Orientation changed signal had incorrect parameters: "
                 << signal->ToString();
    return;
  }
  orientation.upward = static_cast<ScreenOrientation::Side>(upward);

  Provider::GetInstance()->ScreenOrientationChanged(orientation);
}

void SensorsSourceChromeos::OrientationChangedConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!success)
    LOG(WARNING) << "Failed to connect to orientation changed signal.";
}

}  // namespace sensors
