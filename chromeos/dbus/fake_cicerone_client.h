// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_CICERONE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_CICERONE_CLIENT_H_

#include "base/observer_list.h"
#include "chromeos/dbus/cicerone_client.h"

namespace chromeos {

// FakeCiceroneClient is a fake implementation of CiceroneClient used for
// testing.
class CHROMEOS_EXPORT FakeCiceroneClient : public CiceroneClient {
 public:
  FakeCiceroneClient();
  ~FakeCiceroneClient() override;

  // CiceroneClient overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // IsContainerStartedSignalConnected must return true before StartContainer
  // is called.
  bool IsContainerStartedSignalConnected() override;

  // Fake version of the method that launches an application inside a running
  // Container. |callback| is called after the method call finishes.
  void LaunchContainerApplication(
      const vm_tools::cicerone::LaunchContainerApplicationRequest& request,
      DBusMethodCallback<vm_tools::cicerone::LaunchContainerApplicationResponse>
          callback) override;

  // Fake version of the method that gets application icons from inside a
  // Container. |callback| is called after the method call finishes.
  void GetContainerAppIcons(
      const vm_tools::cicerone::ContainerAppIconRequest& request,
      DBusMethodCallback<vm_tools::cicerone::ContainerAppIconResponse> callback)
      override;

  // Fake version of the method that waits for the Cicerone service to be
  // availble.  |callback| is called after the method call finishes.
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;

  // Set ContainerStartedSignalConnected state
  void set_container_started_signal_connected(bool connected) {
    is_container_started_signal_connected_ = connected;
  }

 protected:
  void Init(dbus::Bus* bus) override {}

 private:
  bool is_container_started_signal_connected_ = true;
  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(FakeCiceroneClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_CICERONE_CLIENT_H_
