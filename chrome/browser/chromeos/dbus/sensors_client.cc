// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/sensors_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/system/runtime_environment.h"
#include "content/public/browser/browser_thread.h"
#include "content/browser/sensors/sensors_provider.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

using content::BrowserThread;

// TODO(cwolfe): Fix the DEPs so that these can be pulled in from
//               "chromeos/dbus/service_constants.h".
namespace chromeos {
// Sensors service identifiers.
const char kSensorsServiceName[] = "org.chromium.Sensors";
const char kSensorsServicePath[] = "/org/chromium/Sensors";
const char kSensorsServiceInterface[] = "org.chromium.Sensors";
// Sensors signal names.
const char kScreenOrientationChanged[] = "ScreenOrientationChanged";

// The SensorsClient implementation used in production.
class SensorsClientImpl : public SensorsClient {
 public:
  explicit SensorsClientImpl(dbus::Bus* bus)
      : sensors_proxy_(NULL),
        weak_ptr_factory_(this) {
    sensors_proxy_ = bus->GetObjectProxy(chromeos::kSensorsServiceName,
                                         chromeos::kSensorsServicePath);
    sensors_proxy_->ConnectToSignal(
        chromeos::kSensorsServiceInterface,
        chromeos::kScreenOrientationChanged,
        base::Bind(&SensorsClientImpl::OrientationChangedReceived,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&SensorsClientImpl::OrientationChangedConnected,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  virtual ~SensorsClientImpl() {
  }

 private:
  void OrientationChangedReceived(dbus::Signal* signal) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    content::ScreenOrientation orientation;

    dbus::MessageReader reader(signal);
    int32 upward = 0;
    if (!reader.PopInt32(&upward)) {
      LOG(WARNING) << "Orientation changed signal had incorrect parameters: "
                   << signal->ToString();
      return;
    }
    VLOG(1) << "Orientation changed to upward " << upward;
    orientation = static_cast<content::ScreenOrientation>(upward);

    sensors::Provider::GetInstance()->ScreenOrientationChanged(orientation);
  }

  void OrientationChangedConnected(
      const std::string& interface_name,
      const std::string& signal_name,
      bool success) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (!success)
      LOG(WARNING) << "Failed to connect to orientation changed signal.";
  }

  dbus::ObjectProxy* sensors_proxy_;
  base::WeakPtrFactory<SensorsClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SensorsClientImpl);
};

// The SensorsClient implementation used on Linux desktop,
// which does nothing.
class SensorsClientStubImpl : public SensorsClient {
};

SensorsClient::SensorsClient() {
}

SensorsClient::~SensorsClient() {
}

SensorsClient* SensorsClient::Create(dbus::Bus* bus) {
  if (system::runtime_environment::IsRunningOnChromeOS()) {
    return new SensorsClientImpl(bus);
  } else {
    return new SensorsClientStubImpl();
  }
}

}  // namespace chromeos
